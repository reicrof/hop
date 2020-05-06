#include "common/Profiler.h"
#include "common/Utils.h"

#include "miniz.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <numeric> // accumulate

namespace hop
{
Profiler::Profiler( SourceType type, int processId, const char* str )
    : _name( str ),
      _recording( false ),
      _srcType( type ),
      _loadedFileCpuFreqGHz( 0 ),
      _earliestTimeStamp( 0 ),
      _latestTimeStamp( 0 )
{
   if( type == Profiler::SRC_TYPE_PROCESS )
      _server.start( processId , _name.c_str());
}

const char* Profiler::nameAndPID( int* processId ) const
{
   return _server.processInfo( processId );
}

float Profiler::cpuFreqGHz() const
{
   if( _srcType == Profiler::SRC_TYPE_PROCESS )
   {
      return _server.cpuFreqGHz();
   }

   // If we are not profiling a process, we have opened a file, and we should return the value read
   return _loadedFileCpuFreqGHz;
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

hop_connection_state Profiler::connectionState() const
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

hop_timestamp_t Profiler::earliestTimestamp() const
{
   return _earliestTimeStamp;
}

hop_timestamp_t Profiler::latestTimestamp() const
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
   HOP_ENTER_FUNC( 0 );

   _server.getPendingData( _serverPendingData );

   if ( _recording )
   {
      HOP_ENTER( "Fetching Str Data", 0 );
      addStringData( _serverPendingData.stringData );
      HOP_LEAVE();

      HOP_ENTER( "Fetching Traces", 0 );
      for( const auto& threadTraces : _serverPendingData.tracesPerThread )
      {
         addTraces( threadTraces.second, threadTraces.first );
      }
      HOP_LEAVE();

      HOP_ENTER( "Fetching Lock Waits", 0 );
      for( const auto& lockwaits : _serverPendingData.lockWaitsPerThread )
      {
         addLockWaits( lockwaits.second, lockwaits.first );
      }
      HOP_LEAVE();

      HOP_ENTER( "Fetching Unlock Events", 0 );
      for( const auto& unlockEvents : _serverPendingData.unlockEventsPerThread )
      {
         addUnlockEvents( unlockEvents.second, unlockEvents.first );
      }
      HOP_LEAVE();

      HOP_ENTER( "Fetching CoreEvents", 0 );
      for( const auto& coreEvents : _serverPendingData.coreEventsPerThread )
      {
         addCoreEvents( coreEvents.second, coreEvents.first );
      }
      HOP_LEAVE();
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
      const hop_timestamp_t newEarliestTime =
         *std::min_element( traces.entries.starts.begin(), traces.entries.starts.end() );
      // Set the timeline absolute start time to this new value
      if ( _earliestTimeStamp == 0 || newEarliestTime < _earliestTimeStamp )
         _earliestTimeStamp = newEarliestTime;
   }

   _tracks[threadIndex].addTraces( traces );
}

void Profiler::addStringData( const std::vector<char>& strData )
{
   HOP_ENTER_FUNC( 0 );
   // We should read the string data even when not recording since the string data
   // is sent only once (the first time a function is used)
   if ( !strData.empty() )
   {
      _strDb.addStringData( strData );
   }
   HOP_LEAVE();
}

void Profiler::addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex )
{
   HOP_ENTER_FUNC( 0 );
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !lockWaits.entries.ends.empty() )
   {
      _tracks[threadIndex].addLockWaits( lockWaits );
   }
   HOP_LEAVE();
}

void Profiler::addUnlockEvents( const std::vector<hop_unlock_event_t>& unlockEvents, uint32_t threadIndex )
{
   HOP_ENTER_FUNC( 0 );
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !unlockEvents.empty() )
   {
      _tracks[threadIndex].addUnlockEvents( unlockEvents );
   }
   HOP_LEAVE();
}

void Profiler::addCoreEvents( const CoreEventData& coreEvents, uint32_t threadIndex )
{
   HOP_ENTER_FUNC( 0 );
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !coreEvents.cores.empty() )
   {
      _tracks[threadIndex].addCoreEvents( coreEvents );
   }
   HOP_LEAVE();
}

void Profiler::addThreadName( hop_str_ptr_t name, uint32_t threadIndex )
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
   float    version;
   float    cpuFreqGHz;
   uint64_t uncompressedSize;
   uint32_t strDbSize;
   uint32_t threadCount;
};

bool hop::Profiler::saveToFile( const char* savePath )
{
   HOP_ENTER( "Serializing", 0 );
   setRecording( false );
   // Compute the size of the serialized data
   const mz_ulong dbSerializedSize = serializedSize( _strDb );
   mz_ulong timelineTracksSerializedSize = 0;
   for( size_t i = 0; i < _tracks.size(); ++i )
   {
      timelineTracksSerializedSize += serializedSize( _tracks[i] );
   }

   const mz_ulong totalSerializedSize = timelineTracksSerializedSize + dbSerializedSize;

   std::vector<char> data( totalSerializedSize );

   size_t index = serialize( _strDb, &data[0] );
   for( size_t i = 0; i < _tracks.size(); ++i )
   {
      index += serialize( _tracks[i], &data[index] );
   }
   HOP_LEAVE();

   HOP_ENTER( "Compressing", 0 );
   mz_ulong compressedSize = compressBound( totalSerializedSize );
   std::vector<char> compressedData( compressedSize );
   int compressionStatus = compress(
       (unsigned char*)compressedData.data(),
       &compressedSize,
       (const unsigned char*)&data[0],
       totalSerializedSize );
   HOP_LEAVE();

   if( compressionStatus != Z_OK )
   {
      fprintf( stderr, "Compression failed. File not saved!" );
      return false;
   }

   HOP_ENTER( "Writing to disk", 0 );
   std::ofstream of( savePath, std::ofstream::binary );
   if( of.is_open() )
   {
      SaveFileHeader header = {MAGIC_NUMBER,
                               HOP_VERSION,
                               cpuFreqGHz(),
                               totalSerializedSize,
                               (uint32_t)dbSerializedSize,
                               (uint32_t)_tracks.size()};
      of.write( (const char*)&header, sizeof( header ) );
      of.write( &compressedData[0], compressedSize );
   }
   HOP_LEAVE();

   return of.good();
}

bool hop::Profiler::openFile( const char* path )
{
   HOP_ENTER( "Reading file", 0 );
   std::ifstream input( path, std::ifstream::binary );
   if( !input.is_open() ) return false;

   clear();  // Remove any existing data

   std::vector<char> data(
       ( std::istreambuf_iterator<char>( input ) ), ( std::istreambuf_iterator<char>() ) );

   HOP_LEAVE();

   SaveFileHeader* header = (SaveFileHeader*)&data[0];
   if( header->magicNumber != MAGIC_NUMBER )
   {
      fprintf(stderr, "Magic number does not match\n" );
      return false;
   }

   if( header->version != HOP_VERSION )
   {
      fprintf(
          stderr,
          "Hop file version %f does not match viewer version %f\n",
          header->version,
          HOP_VERSION );
      return false;
   }
   

   HOP_ENTER( "Uncompressing", 0 );
   std::vector<char> uncompressedData( header->uncompressedSize );
   mz_ulong uncompressedSize = uncompressedData.size();

   int uncompressStatus = uncompress(
       (unsigned char*)uncompressedData.data(),
       &uncompressedSize,
       (unsigned char*)&data[sizeof( SaveFileHeader )],
       data.size() - sizeof( SaveFileHeader ) );
   HOP_LEAVE();

   if( uncompressStatus != Z_OK )
   {
      return false;
   }

   HOP_ENTER( "Updating data", 0 );

   _loadedFileCpuFreqGHz = header->cpuFreqGHz;

   size_t i            = 0;
   const size_t dbSize = deserialize( &uncompressedData[i], _strDb );
   assert( dbSize == header->strDbSize );
   i += dbSize;

   std::vector<TimelineTrack> timelineTracks( header->threadCount );
   for( uint32_t j = 0; j < header->threadCount; ++j )
   {
      size_t timelineTrackSize = deserialize( &uncompressedData[i], timelineTracks[j] );
      addTraces( timelineTracks[j]._traces, j );
      addLockWaits( timelineTracks[j]._lockWaits, j );
      addCoreEvents( timelineTracks[j]._coreEvents, j );
      i += timelineTrackSize;
   }
   _srcType = SRC_TYPE_FILE;

   HOP_LEAVE();

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