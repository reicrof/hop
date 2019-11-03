#include "Profiler.h"

#include <algorithm>
#include <cassert>

namespace hop
{
Profiler::Profiler( SourceType type, int processId, const char* str )
    : _srcType( type ),
      _pid( processId ),
      _name( str ),
      _earliestTimeStamp( 0 ),
      _latestTimeStamp( 0 )
{
}

const char* Profiler::nameAndPID( int* processId )
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

   _tracks[threadIndex].setTrackName( name );
}

Profiler::~Profiler() { _server.stop(); }

}  // namespace hop