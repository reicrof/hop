#include "Profiler.h"

#include "miniz.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <numeric> // accumulate

namespace hop
{
Profiler::Profiler( SourceType type, int processId, const char* str )
    : _name( str ),
      _pid( processId ),
      _srcType( type ),
      _earliestTimeStamp( 0 ),
      _latestTimeStamp( 0 )
{
   _server.start( _pid , _name.c_str());
}

const char* Profiler::nameAndPID( int* processId ) const
{
   if( processId ) *processId = _pid;
   return _name.c_str();
}

Profiler::SourceType Profiler::sourceType() const { return _srcType; }

void Profiler::setRecording( bool recording )
{
   _recording = recording;
   _server.setRecording( recording );
}

bool Profiler::recording() const
{
   return _recording;
}

SharedMemory::ConnectionState Profiler::connectionState() const
{
   return _server.connectionState();
}

const std::vector<TimelineTrack>& Profiler::timelineTracks() const
{
   return _tracks;
}

const StringDb& Profiler::stringDb() const
{
   return _strDb;
}

TimeStamp Profiler::earliestTimestamp() const
{
   return _earliestTimeStamp;
}

TimeStamp Profiler::latestTimestamp() const
{
   return _latestTimeStamp;
}

ProfilerStats Profiler::stats() const
{
   ProfilerStats stats = {};
   stats.strDbSize = _strDb.sizeInBytes();
   stats.clientSharedMemSize = _server.sharedMemorySize();
   for ( size_t i = 0; i < _tracks.size(); ++i )
   {
      stats.traceCount += _tracks[i]._traces.entries.ends.size();
   }

   return stats;
}

void Profiler::fetchClientData()
{
   HOP_PROF_FUNC();

   _server.getPendingData( _serverPendingData );

   if ( _recording )
   {
      HOP_PROF_SPLIT( "Fetching Str Data" );

      addStringData( _serverPendingData.stringData );

      HOP_PROF_SPLIT( "Fetching Traces" );
      for( const auto& threadTraces : _serverPendingData.tracesPerThread )
      {
         addTraces( threadTraces.second, threadTraces.first );
      }
      HOP_PROF_SPLIT( "Fetching Lock Waits" );
      for( const auto& lockwaits : _serverPendingData.lockWaitsPerThread )
      {
         addLockWaits( lockwaits.second, lockwaits.first );
      }
      HOP_PROF_SPLIT( "Fetching Unlock Events" );
      for( const auto& unlockEvents : _serverPendingData.unlockEventsPerThread )
      {
         addUnlockEvents( unlockEvents.second, unlockEvents.first );
      }
      HOP_PROF_SPLIT( "Fetching CoreEvents" );
      for( const auto& coreEvents : _serverPendingData.coreEventsPerThread )
      {
         addCoreEvents( coreEvents.second, coreEvents.first );
      }
   }

   // We need to get the thread name even when not recording as they are only sent once
   for ( size_t i = 0; i < _serverPendingData.threadNames.size(); ++i )
   {
      addThreadName(
          _serverPendingData.threadNames[i].second, _serverPendingData.threadNames[i].first );
   }
}

void Profiler::addTraces( const TraceData& traces, uint32_t threadIndex )
{
   // Ignore empty traces
   if ( traces.entries.ends.empty() ) return;

   // Add new thread as they come
   if ( threadIndex >= _tracks.size() )
   {
      static_assert( std::is_move_constructible<TimelineTrack>(), "TimelineTrack is not move assignable" );
      _tracks.resize( threadIndex + 1 );
   }

   // Update the current time
   _latestTimeStamp = std::max( traces.entries.ends.back(), _latestTimeStamp );

   // If this is the first traces received from the thread, update the
   // start time as it may be earlier.
   if ( _tracks[threadIndex]._traces.entries.ends.empty() )
   {
      // Find the earliest trace
      TimeStamp earliestTime = traces.entries.ends[0] - traces.entries.deltas[0];
      for ( size_t i = 1; i < traces.entries.ends.size(); ++i )
      {
         earliestTime = std::min( earliestTime, traces.entries.ends[i] - traces.entries.deltas[i] );
      }
      // Set the timeline absolute start time to this new value
      if ( _earliestTimeStamp == 0 || earliestTime < _earliestTimeStamp )
         _earliestTimeStamp = earliestTime;
   }

   _tracks[threadIndex].addTraces( traces );
}

void Profiler::addStringData( const std::vector<char>& strData )
{
   HOP_PROF_FUNC();
   // We should read the string data even when not recording since the string data
   // is sent only once (the first time a function is used)
   if ( !strData.empty() )
   {
      _strDb.addStringData( strData );
   }
}

void Profiler::addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !lockWaits.entries.ends.empty() )
   {
      _tracks[threadIndex].addLockWaits( lockWaits );
   }
}

void Profiler::addUnlockEvents( const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !unlockEvents.empty() )
   {
      _tracks[threadIndex].addUnlockEvents( unlockEvents );
   }
}

void Profiler::addCoreEvents( const std::vector<CoreEvent>& coreEvents, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !coreEvents.empty() )
   {
      _tracks[threadIndex].addCoreEvents( coreEvents );
   }
}

void Profiler::addThreadName( StrPtr_t name, uint32_t threadIndex )
{
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   assert( name != 0 );  // should not be empty name

   _tracks[threadIndex].setName( name );
}

const uint32_t MAGIC_NUMBER = 1095780676;  // "DIPA"
struct SaveFileHeader
{
   uint32_t magicNumber;
   uint32_t version;
   size_t uncompressedSize;
   uint32_t strDbSize;
   uint32_t threadCount;
};

bool hop::Profiler::saveToFile( const char* savePath )
{
   setRecording( false );
   // Compute the size of the serialized data
   const size_t dbSerializedSize       = serializedSize( _strDb );
   std::vector<size_t> timelineTrackSerializedSize( _tracks.size() );
   for( size_t i = 0; i < _tracks.size(); ++i )
   {
      timelineTrackSerializedSize[i] = serializedSize( _tracks[i] );
   }

   const size_t totalSerializedSize =
       std::accumulate(
           timelineTrackSerializedSize.begin(), timelineTrackSerializedSize.end(), size_t{0} ) + dbSerializedSize;

   std::vector<char> data( totalSerializedSize );

   size_t index = serialize( _strDb, &data[0] );
   for( size_t i = 0; i < _tracks.size(); ++i )
   {
      index += serialize( _tracks[i], &data[index] );
   }

   mz_ulong compressedSize = compressBound( totalSerializedSize );
   std::vector<char> compressedData( compressedSize );
   int compressionStatus = compress(
       (unsigned char*)compressedData.data(),
       &compressedSize,
       (const unsigned char*)&data[0],
       totalSerializedSize );
   if( compressionStatus != Z_OK )
   {
      fprintf( stderr, "Compression failed. File not saved!" );
      return false;
   }

   std::ofstream of( savePath, std::ofstream::binary );
   SaveFileHeader header = {
       MAGIC_NUMBER, 1, totalSerializedSize, (uint32_t)dbSerializedSize, (uint32_t)_tracks.size()};
   of.write( (const char*)&header, sizeof( header ) );
   of.write( &compressedData[0], compressedSize );

   return true;
}

void Profiler::clear()
{
   _server.clear();
   _strDb.clear();
   _tracks.clear();
   _recording = false;
}

Profiler::~Profiler() { _server.stop(); }

}  // namespace hop