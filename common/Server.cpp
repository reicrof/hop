#include "common/Server.h"
#include "common/Utils.h"
#include "common/platform/Platform.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <algorithm>
#include <chrono>

template <typename T, class BinaryPredicate, class MergeFct>
static T merge_consecutive( T first, T last, BinaryPredicate pred, MergeFct merge )
{
   int mergeCount = 0;
   auto writePos = first;
   while ( ++first != last )
   {
      if ( !pred( *writePos, *first ) )
      {
         merge( *( first - 1 ), *writePos );
         std::swap( *( first - 1 ), *writePos );
         writePos = first;
         ++mergeCount;
      }
   }
   merge( *( first - 1 ), *writePos );
   std::swap( *( first - 1 ), *writePos );

   return last - mergeCount;
}

static int mergeAndRemoveDuplicates( hop::CoreEvent* coreEvents, uint32_t count )
{
   // Merge events that are less than 10 micro apart
   const uint64_t minCycles = hop::nanosToCycles( 10000 );
   auto canMergeCore = [minCycles]( const hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) {
      return lhs.core == rhs.core &&
             ( ( rhs.start < lhs.end || ( rhs.start - lhs.end ) < minCycles ) );
   };

   auto sameCore = []( const hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) {
      return lhs.core == rhs.core;
   };

   auto mergedEnd = merge_consecutive(
       coreEvents,
       coreEvents + count,
       canMergeCore,
       []( hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) { lhs.start = rhs.start; } );
   auto newEnd = std::unique( coreEvents, mergedEnd, sameCore );
   return std::distance( coreEvents, newEnd );
}

namespace hop
{
bool Server::start( int processId, const char* name )
{
   assert( name != nullptr );

   _state.running = true;
   _state.pid = -1;
   _state.processName = name;
   _state.connectionState = SharedMemory::NOT_CONNECTED;

   // Swap for ptr owned by the state
   name = _state.processName.c_str();

   _thread = std::thread( [this, processId, name]() {
      TimeStamp lastSignalTime = getTimeStamp();
      SharedMemory::ConnectionState prevConnectionState = SharedMemory::NOT_CONNECTED;

      const uint32_t MAX_RECONNECT_TIMEOUT_MS = 500;
      uint32_t reconnectTimeoutMs = 10;
      while ( true )
      {
         // Try to get the shared memory
         if ( !_sharedMem.valid() )
         {
            HOP_PROF( "Trying to open process..." );
            const hop::ProcessInfo procInfo = processId != -1
                                                  ? hop::getProcessInfoFromPID( processId )
                                                  : hop::getProcessInfoFromProcessName( name );
            SharedMemory::ConnectionState state =
                _sharedMem.create( procInfo.pid, 0 /*will be define in shared metadata*/, true );

            if ( state != SharedMemory::CONNECTED )
            {
               { // Update state then go to sleep a few MS
                  std::lock_guard<hop::Mutex> guard( _stateMutex );
                  _state.connectionState = state;
                  if( !_state.running )
                     return;  // We are done without even opening the shared mem :(
               }

               prevConnectionState = state;
               // Sleep few ms before retrying. Increase timeout time each try
               std::this_thread::sleep_for( std::chrono::milliseconds( reconnectTimeoutMs ) );
               reconnectTimeoutMs = std::min( reconnectTimeoutMs + 10, MAX_RECONNECT_TIMEOUT_MS );
               continue;
            }

            // Clear any remaining messages from previous execution now
            clearPendingMessages();
            _sharedMem.setListeningConsumer( _state.recording );

            std::lock_guard<hop::Mutex> guard( _stateMutex );
            _state.connectionState = state;
            _state.pid             = procInfo.pid;
            _state.processName     = std::string( procInfo.name );
            prevConnectionState    = state;

            // Set HOP thread name
            char serverName[64];
            snprintf( serverName, sizeof( serverName ), "%s [Producer]", _state.processName.c_str() );
            HOP_SET_THREAD_NAME( serverName );

            printf( "Connection to shared data successful.\n" );
         }

         HOP_PROF_FUNC();
         const bool wasSignaled = _sharedMem.tryWaitSemaphore();

         // Check if its been a while since we have been signaleds
         const TimeStamp curTime = getTimeStamp();
         const bool producerLost =
             !_sharedMem.hasConnectedProducer() || curTime - lastSignalTime > 3000000000;
         const auto newConnectionState =
             producerLost ? SharedMemory::CONNECTED_NO_CLIENT : SharedMemory::CONNECTED;

         // Check and update current server state
         {
            std::lock_guard<hop::Mutex> guard( _stateMutex );

            //  Stop processing if we are done running
            if( !_state.running ) break;

            // A clear was requested so we need to clear our string database
            if( _state.clearingRequested )
            {
               _stringDb.clear();
               clearPendingMessages();
               _state.clearingRequested = false;
               _sharedMem.setResetTimestamp( getTimeStamp() );
               _threadNamesReceived.clear();
               continue;
            }

            // Update state if it has changed
            if( prevConnectionState != newConnectionState )
            {
               prevConnectionState    = newConnectionState;
               _state.connectionState = newConnectionState;
            }
         }

         if ( !wasSignaled )
         {
            // We timed out.

            // If we have a connected producer and we timed out, it simply means the app is either
            // not very productive or is being debuged. If we do not have a producer, it means the
            // app was closed.
            using namespace std::chrono;
            const auto timeToSleep =
                _sharedMem.hasConnectedProducer() ? milliseconds( 100 ) : milliseconds( 1000 );
            std::this_thread::sleep_for( timeToSleep );
            continue;
         }

         // We were signaled
         lastSignalTime = curTime;

         size_t offset = 0;
         const size_t bytesToRead = ringbuf_consume( _sharedMem.ringbuffer(), &offset );
         if ( bytesToRead > 0 )
         {
            HOP_PROF( "Server - Handling new messages" );
            const TimeStamp minTimestamp = _sharedMem.lastResetTimestamp();
            size_t bytesRead = 0;
            while ( bytesRead < bytesToRead )
            {
               bytesRead += handleNewMessage(
                   &_sharedMem.data()[offset + bytesRead], bytesToRead - bytesRead, minTimestamp );
            }
            ringbuf_release( _sharedMem.ringbuffer(), bytesToRead );
         }
      }
   } );

   return true;
}

const char* Server::processInfo( int* processId ) const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   static const char* noProcessStr = "<No Process>";
   if( processId ) *processId = _state.pid;
   return _state.processName.empty() ? noProcessStr : _state.processName.c_str();
}

SharedMemory::ConnectionState Server::connectionState() const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   return _state.connectionState;
}

size_t Server::sharedMemorySize() const
{
   if ( _sharedMem.valid() )
   {
      return _sharedMem.sharedMetaInfo()->requestedSize;
   }

   return 0;
}

void Server::setRecording( bool recording )
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _state.recording = recording;
   if ( _sharedMem.valid() )
   {
      _sharedMem.setListeningConsumer( recording );
   }
}

void Server::getPendingData( PendingData& data )
{
   HOP_PROF_FUNC();
   std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
   _sharedPendingData.swap( data );
   _sharedPendingData.clear();
}

bool Server::addUniqueThreadName( uint32_t threadIndex, StrPtr_t name )
{
   bool newInsert = false;
   if ( _threadNamesReceived.size() <= threadIndex )
   {
      _threadNamesReceived.resize( threadIndex + 1, 0 );
   }

   if ( _threadNamesReceived[threadIndex] == 0 )
   {
      _threadNamesReceived[threadIndex] = name;
      newInsert = true;
   }

   return newInsert;
}

size_t Server::handleNewMessage( uint8_t* data, size_t maxSize, TimeStamp minTimestamp )
{
   uint8_t* bufPtr = data;
   const MsgInfo* msgInfo = (const MsgInfo*)bufPtr;
   const MsgType msgType = msgInfo->type;
   const uint32_t threadIndex = msgInfo->threadIndex;

   bufPtr += sizeof( MsgInfo );
   assert( ( size_t )( bufPtr - data ) <= maxSize );
   (void)maxSize;  // Removed unused warning

   // If the message was sent prior to the last reset timestamp, ignore it
   if ( msgInfo->timeStamp < minTimestamp ) { return maxSize; }

   // If the thread has an assigned name
   if ( msgInfo->threadName != 0 && addUniqueThreadName( threadIndex, msgInfo->threadName ) )
   {
      std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
      _sharedPendingData.threadNames.emplace_back( threadIndex, msgInfo->threadName );
    }

    switch ( msgType )
    {
       case MsgType::PROFILER_STRING_DATA:
       {
          // Copy string and add it to database
          const size_t strSize = msgInfo->stringData.size;
          if ( strSize > 0 )
          {
             const char* strDataPtr = (const char*)bufPtr;
             bufPtr += strSize;
             assert( ( size_t )( bufPtr - data ) <= maxSize );

             // TODO: Could lock later when we received all the messages
             std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
             _stringDb.addStringData( strDataPtr, strSize );
             _sharedPendingData.stringData.insert(
                 _sharedPendingData.stringData.end(), strDataPtr, strDataPtr + strSize );
          }
          return ( size_t )( bufPtr - data );
       }
       case MsgType::PROFILER_TRACE:
       {
          const size_t tracesCount = msgInfo->traces.count;
          if ( tracesCount > 0 )
          {
             TraceData traceData;
             const TimeStamp* starts = (const TimeStamp*)bufPtr;
             const TimeStamp* ends = starts + tracesCount;
             const StrPtr_t* fileNames = (const StrPtr_t*)( ends + tracesCount );
             const StrPtr_t* fctNames = fileNames + tracesCount;
             const LineNb_t* lineNbs = (const LineNb_t*)( fctNames + tracesCount );
             const Depth_t* depths = (const Depth_t*)( lineNbs + tracesCount );
             const ZoneId_t* zones = (const ZoneId_t*)( depths + tracesCount );

             traceData.entries.ends.insert(
                 traceData.entries.ends.end(), ends, ends + tracesCount );
             traceData.entries.starts.insert(
                 traceData.entries.starts.end(), starts, starts + tracesCount );
             traceData.entries.depths.insert(
                 traceData.entries.depths.end(), depths, depths + tracesCount );
             traceData.entries.maxDepth = *std::max_element( depths, depths + tracesCount );

             traceData.lineNbs.insert( traceData.lineNbs.end(), lineNbs, lineNbs + tracesCount );
             traceData.zones.insert( traceData.zones.end(), zones, zones + tracesCount );

             // Process non trivially copiable members
             for ( size_t i = 0; i < tracesCount; ++i )
             {
                traceData.fileNameIds.push_back( _stringDb.getStringIndex( fileNames[i] ) );
                traceData.fctNameIds.push_back( _stringDb.getStringIndex( fctNames[i] ) );
             }

             // The ends time should already be sorted
             assert_is_sorted( traceData.entries.ends.begin(), traceData.entries.ends.end() );

             bufPtr +=
                 ( ( sizeof( TimeStamp ) + sizeof( TimeStamp ) + sizeof( Depth_t ) +
                     sizeof( StrPtr_t ) + sizeof( StrPtr_t ) + sizeof( LineNb_t ) +
                     sizeof( ZoneId_t ) ) *
                   tracesCount );
             assert( ( size_t )( bufPtr - data ) <= maxSize );

             static_assert(
                 std::is_move_constructible<TraceData>::value, "Trace Data not moveable" );
             // TODO: Could lock later when we received all the messages
             std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
             _sharedPendingData.tracesPerThread[threadIndex].append( traceData );
          }
          return ( size_t )( bufPtr - data );
       }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         const LockWait* lws = (const LockWait*)bufPtr;
         const uint32_t lwCount = msgInfo->lockwaits.count;

         LockWaitData lockwaitData;
         Depth_t maxDepth = 0;
         for ( uint32_t i = 0; i < lwCount; ++i )
         {
            lockwaitData.entries.ends.push_back( lws[i].end );
            lockwaitData.entries.starts.push_back( lws[i].start );
            lockwaitData.entries.depths.push_back( lws[i].depth );
            lockwaitData.mutexAddrs.push_back( lws[i].mutexAddress );
            maxDepth = std::max( maxDepth, lws[i].depth );
         }
         lockwaitData.entries.maxDepth = maxDepth;

         bufPtr += ( lwCount * sizeof( LockWait ) );

         // The ends time should already be sorted
         assert_is_sorted( lockwaitData.entries.ends.begin(), lockwaitData.entries.ends.end() );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.lockWaitsPerThread[threadIndex].append( lockwaitData );

         return ( size_t )( bufPtr - data );
      }
      case MsgType::PROFILER_UNLOCK_EVENT:
      {
         const size_t eventCount = msgInfo->unlockEvents.count;
         UnlockEvent* eventPtr = (UnlockEvent*)bufPtr;

         bufPtr += eventCount * sizeof( UnlockEvent );
         assert( ( size_t )( bufPtr - data ) <= maxSize );

         std::sort(
             eventPtr, eventPtr + eventCount, []( const UnlockEvent& lhs, const UnlockEvent& rhs ) {
                return lhs.time < rhs.time;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         auto& unlocks = _sharedPendingData.unlockEventsPerThread[threadIndex];
         unlocks.insert( unlocks.end(), eventPtr, eventPtr + eventCount );

         return ( size_t )( bufPtr - data );
      }
      case MsgType::PROFILER_HEARTBEAT:
      {
         return ( size_t )( bufPtr - data );
      }
      case MsgType::PROFILER_CORE_EVENT:
      {
         const size_t eventCount = msgInfo->coreEvents.count;
         CoreEvent* coreEventsPtr = (CoreEvent*)bufPtr;

         // Must be done before removing duplicates
         bufPtr += eventCount * sizeof( CoreEvent );
         assert( ( size_t )( bufPtr - data ) <= maxSize );

         const size_t newCount = mergeAndRemoveDuplicates( coreEventsPtr, eventCount );

         CoreEventData coresData;
         for ( uint32_t i = 0; i < newCount; ++i )
         {
            coresData.entries.ends.push_back( coreEventsPtr[i].end );
            coresData.entries.starts.push_back( coreEventsPtr[i].start );
            coresData.cores.push_back( coreEventsPtr[i].core );
         }
         coresData.entries.depths.insert( coresData.entries.depths.end(), newCount, 0 );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.coreEventsPerThread[threadIndex].append( coresData );
         return ( size_t )( bufPtr - data );
      }
      default:
         assert( false );
         return ( size_t )( bufPtr - data );
   }
}

void Server::clearPendingMessages()
{
   size_t offset = 0;
   while ( size_t bytesToRead = ringbuf_consume( _sharedMem.ringbuffer(), &offset ) )
   {
      ringbuf_release( _sharedMem.ringbuffer(), bytesToRead );
   }
}

void Server::clear()
{
   setRecording( false );

   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _state.clearingRequested = true;
}

void Server::stop()
{
   bool wasRunning = false;
   {
      std::lock_guard<hop::Mutex> guard( _stateMutex );
      if( _state.running )
      {
         wasRunning     = true;
         _state.running = false;
      }
   }

   if( wasRunning )
   {
      if( _sharedMem.valid() )
      {
         _sharedMem.setListeningConsumer( false );
         _sharedMem.setConnectedConsumer( false );
      }
      // Wake up semaphore to close properly
      if( _sharedMem.valid() && _sharedMem.semaphore() )
      {
         _sharedMem.signalSemaphore();
      }
      if( _thread.joinable() )
      {
         _thread.join();
      }
   }
}

void Server::PendingData::clear()
{
   HOP_PROF_FUNC();

   stringData.clear();
   for ( auto& traces : tracesPerThread )
   {
      traces.second.clear();
   }

   for ( auto& lockwaits : lockWaitsPerThread )
   {
      lockwaits.second.clear();
   }

   for ( auto& unlockEvents : unlockEventsPerThread )
   {
      unlockEvents.second.clear();
   }

   for ( auto& coreEvents : coreEventsPerThread )
   {
      coreEvents.second.clear();
   }

   threadNames.clear();
}

void Server::PendingData::swap( PendingData& rhs )
{
   HOP_PROF_FUNC();
   using std::swap;
   swap( tracesPerThread, rhs.tracesPerThread );
   swap( stringData, rhs.stringData );
   swap( lockWaitsPerThread, rhs.lockWaitsPerThread );
   swap( unlockEventsPerThread, rhs.unlockEventsPerThread );
   swap( coreEventsPerThread, rhs.coreEventsPerThread );
   swap( threadNames, rhs.threadNames );
}

}  // namespace hop