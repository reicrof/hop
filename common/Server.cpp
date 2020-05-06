#include "common/Server.h"
#include "common/Utils.h"
#include "common/platform/Platform.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <algorithm>
#include <chrono>

static constexpr int POLL_COUNT_FAIL_DISCONNECTION = 20;

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

static int mergeAndRemoveDuplicates( hop_event* coreEvents, uint32_t count, float cpuGHz )
{
   // Merge events that are less than 10 micro apart
   const uint64_t minCycles = hop::nanosToCycles( 10000, cpuGHz );
   auto canMergeCore        = [minCycles]( const hop_event& lhs, const hop_event& rhs ) {
      return lhs.core.core == rhs.core.core &&
             ( ( rhs.core.start < lhs.core.end || ( rhs.core.start - lhs.core.end ) < minCycles ) );
   };

   auto sameCore = []( const hop_event& lhs, const hop_event& rhs ) {
      return lhs.core.core == rhs.core.core;
   };

   auto mergedEnd = merge_consecutive(
       coreEvents,
       coreEvents + count,
       canMergeCore,
       []( hop_event& lhs, const hop_event& rhs ) { lhs.core.start = rhs.core.start; } );
   auto newEnd = std::unique( coreEvents, mergedEnd, sameCore );
   return std::distance( coreEvents, newEnd );
}

static hop_connection_state updateConnectionState(
    const hop_shared_memory* sharedMem,
    int pollFailCount )
{
   hop_connection_state state = HOP_CONNECTED;
   const bool producerLost =
       !hop_has_connected_producer( sharedMem ) || pollFailCount > POLL_COUNT_FAIL_DISCONNECTION;
   if( producerLost )
   {
      // We have lost the producer. Check if it is becaused the process was terminated or
      // if it is simply not feeling chatty
      state = hop::processAlive( hop_client_pid( sharedMem ) ) ? HOP_CONNECTED_NO_CLIENT
                                                               : HOP_NOT_CONNECTED;
   }
   return state;
}

namespace hop
{
bool Server::start( int inPid, const char* name )
{
   assert( name != nullptr );

   _state.running = true;
   _state.pid = -1;
   _state.processName = name;
   _state.connectionState = HOP_NOT_CONNECTED;

   _thread = std::thread( [this, inPid]() {
      int pollFailedCount = 0;
      hop_connection_state connectionState = HOP_NOT_CONNECTED;

      uint32_t reconnectTimeoutMs = 10;
      while ( true )
      {
         // Try to get the shared memory
         if( !_sharedMem )
         {
            while( connectionState != HOP_CONNECTED && tryConnect( inPid, connectionState ) )
            {
               // Sleep few ms before retrying. Increase timeout time each try
               std::this_thread::sleep_for( std::chrono::milliseconds( reconnectTimeoutMs ) );
               static constexpr uint32_t MAX_RECONNECT_TIMEOUT_MS = 500;
               reconnectTimeoutMs = std::min( reconnectTimeoutMs + 10, MAX_RECONNECT_TIMEOUT_MS );
            }

            if( connectionState != HOP_CONNECTED )
               break;  // No connection was found and we are request to stop trying

            printf( "Connection to shared data successful.\n" );
         }

         HOP_ENTER( "Main Server Loop", 0 );

         // Check if its been a while since we have been signaleds
         const auto newConnectionState =
             updateConnectionState( _sharedMem, pollFailedCount );
         const bool clientAlive = newConnectionState != HOP_NOT_CONNECTED;

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
               hop_update_reset_timestamp( _sharedMem );
               _threadNamesReceived.clear();
               continue;
            }

            // Update state if it has changed
            if( connectionState != newConnectionState )
            {
               connectionState        = newConnectionState;
               _state.connectionState = newConnectionState;
            }

            if( !clientAlive )
            {
               _state.pid = -1;
               hop_destroy_shared_memory( _sharedMem );
               continue;
            }
         }

         size_t bytesToRead = 0;
         if( hop_byte_t* data = hop_consume_shared_memory( _sharedMem, &bytesToRead ) )
         {
            HOP_ENTER( "Server - Handling new messages", 0 );
            pollFailedCount = 0;
            const hop_timestamp_t minTimestamp = hop_reset_timestamp( _sharedMem );
            size_t bytesRead = 0;
            while ( bytesRead < bytesToRead )
            {
               bytesRead +=
                   handleNewMessage( data + bytesRead, bytesToRead - bytesRead, minTimestamp );
            }
            hop_release_shared_memory( _sharedMem, bytesToRead );
            HOP_LEAVE();
         }
         else
         {
            HOP_ENTER( "Nothing sent...", 0 );
            // Nothing was sent
            ++pollFailedCount;

            // If we have a connected producer and we timed out, it simply means the app is either
            // not very productive or is being debuged. If we do not have a producer, it means the
            // app was closed.
            using namespace std::chrono;
            if( clientAlive )
            {
               // Relax our polling after a few attemps
               std::this_thread::sleep_for( microseconds( 500 ) * std::min( pollFailedCount, 10 ) );
            }
            else
            {
               std::this_thread::sleep_for( milliseconds( 500 ) );
            }

            HOP_LEAVE();
         }
      }
   } );

   return true;
}

bool Server::tryConnect( int32_t pid, hop_connection_state& newState )
{
   const hop::ProcessInfo procInfo =
       pid != -1 ? hop::getProcessInfoFromPID( pid )
                   : hop::getProcessInfoFromProcessName( _state.processName.c_str() );
   _sharedMem = hop_create_shared_memory(
       procInfo.pid, 0 /*will be define in shared metadata*/, true, &newState );

   if( newState != HOP_CONNECTED )
   {
      {  // Update state then go to sleep a few MS
         std::lock_guard<hop::Mutex> guard( _stateMutex );
         _state.connectionState = newState;
         if( !_state.running ) return false;  // We are done without even opening the shared mem :(
      }

      // Signal we should retry
      return true;
   }

   // Clear any remaining messages from previous execution now
   clearPendingMessages();

   std::lock_guard<hop::Mutex> guard( _stateMutex );
   hop_set_listening_consumer( _sharedMem, _state.recording );

   _state.connectionState = newState;
   _state.pid             = procInfo.pid;
   _state.processName     = std::string( procInfo.name );

   // Set HOP thread name
   char serverName[64];
   snprintf( serverName, sizeof( serverName ), "%s [Producer]", _state.processName.c_str() );
   HOP_SET_THREAD_NAME( serverName );

   // Got the connection, signal that we should to retry
   return false;
}

const char* Server::processInfo( int* processId ) const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   static const char* noProcessStr = "<No Process>";
   if( processId ) *processId = _state.pid;
   return _state.processName.empty() ? noProcessStr : _state.processName.c_str();
}

hop_connection_state Server::connectionState() const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   return _state.connectionState;
}

size_t Server::sharedMemorySize() const
{
   return hop_ipc_memory_size( _sharedMem );
}

float Server::cpuFreqGHz() const
{
   return hop_client_tsc_frequency( _sharedMem );
}

void Server::setRecording( bool recording )
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _state.recording = recording;
   hop_set_listening_consumer( _sharedMem, recording );
}

void Server::getPendingData( PendingData& data )
{
   HOP_ENTER_FUNC( 0 );
   std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
   _sharedPendingData.swap( data );
   _sharedPendingData.clear();
   HOP_LEAVE();
}

bool Server::addUniqueThreadName( uint32_t threadIndex, hop_str_ptr_t name )
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

size_t Server::handleNewMessage( uint8_t* data, size_t maxSize, hop_timestamp_t minTimestamp )
{
   uint8_t* bufPtr             = data;
   const hop_msg_info_t* msgInfo = (const hop_msg_info_t*)bufPtr;
   const hop_msg_type msgType  = msgInfo->type;
   const uint32_t threadIndex  = msgInfo->threadIndex;

   bufPtr += sizeof( hop_msg_info_t );
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
       case HOP_PROFILER_STRING_DATA:
       {
          // Copy string and add it to database
          const size_t strSize = msgInfo->count;
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
       case HOP_PROFILER_TRACE:
       {
          const size_t tracesCount = msgInfo->count;
          if ( tracesCount > 0 )
          {
             TraceData traceData;
             const hop_timestamp_t* starts = (const hop_timestamp_t*)bufPtr;
             const hop_timestamp_t* ends = starts + tracesCount;
             const hop_str_ptr_t* fileNames = (const hop_str_ptr_t*)( ends + tracesCount );
             const hop_str_ptr_t* fctNames = fileNames + tracesCount;
             const hop_linenb_t* lineNbs = (const hop_linenb_t*)( fctNames + tracesCount );
             const hop_depth_t* depths = (const hop_depth_t*)( lineNbs + tracesCount );
             const hop_zone_t* zones = (const hop_zone_t*)( depths + tracesCount );

             traceData.entries.ends.append( ends, ends + tracesCount );
             traceData.entries.starts.append( starts, starts + tracesCount );
             traceData.entries.depths.append( depths, depths + tracesCount );
             traceData.entries.maxDepth = *std::max_element( depths, depths + tracesCount );

             traceData.lineNbs.append( lineNbs, lineNbs + tracesCount );
             traceData.zones.append( zones, zones + tracesCount );

             // Process non trivially copiable members
             for ( size_t i = 0; i < tracesCount; ++i )
             {
                traceData.fileNameIds.push_back( _stringDb.getStringIndex( fileNames[i] ) );
                traceData.fctNameIds.push_back( _stringDb.getStringIndex( fctNames[i] ) );
             }

             // The ends time should already be sorted
             assert_is_sorted( traceData.entries.ends.begin(), traceData.entries.ends.end() );

             bufPtr +=
                 ( ( sizeof( hop_timestamp_t ) + sizeof( hop_timestamp_t ) + sizeof( hop_depth_t ) +
                     sizeof( hop_str_ptr_t ) + sizeof( hop_str_ptr_t ) + sizeof( hop_linenb_t ) +
                     sizeof( hop_zone_t ) ) *
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
      case HOP_PROFILER_WAIT_LOCK:
      {
         const size_t eventCount = msgInfo->count;
         hop_event* events = (hop_event*)bufPtr;

         bufPtr += eventCount * sizeof( hop_event );
         assert( ( size_t )( bufPtr - data ) <= maxSize );

         LockWaitData lockwaitData;
         hop_depth_t maxDepth = 0;
         for ( uint32_t i = 0; i < eventCount; ++i )
         {
            lockwaitData.entries.ends.push_back( events[i].lock_wait.end );
            lockwaitData.entries.starts.push_back( events[i].lock_wait.start );
            lockwaitData.entries.depths.push_back( events[i].lock_wait.depth );
            lockwaitData.mutexAddrs.push_back( events[i].lock_wait.mutexAddress );
            maxDepth = std::max( maxDepth, events[i].lock_wait.depth );
         }
         lockwaitData.entries.maxDepth = maxDepth;

         // The ends time should already be sorted
         assert_is_sorted( lockwaitData.entries.ends.begin(), lockwaitData.entries.ends.end() );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.lockWaitsPerThread[threadIndex].append( lockwaitData );

         return ( size_t )( bufPtr - data );
      }
      case HOP_PROFILER_UNLOCK_EVENT:
      {
         const size_t eventCount = msgInfo->count;
         hop_event* events = (hop_event*)bufPtr;

         bufPtr += eventCount * sizeof( hop_event );
         assert( ( size_t )( bufPtr - data ) <= maxSize );

         std::sort(
             events, events + eventCount, []( const hop_event& lhs, const hop_event& rhs ) {
                return lhs.unlock.time < rhs.unlock.time;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         auto& unlocks = _sharedPendingData.unlockEventsPerThread[threadIndex];
         for( size_t i = 0; i < eventCount; ++i )
         {
            unlocks.emplace_back( events[i].unlock );
         }

         return ( size_t )( bufPtr - data );
      }
      case HOP_PROFILER_CORE_EVENT:
      {
         const size_t eventCount = msgInfo->count;
         hop_event* events = (hop_event*)bufPtr;

         bufPtr += eventCount * sizeof( hop_event );
         assert( ( size_t )( bufPtr - data ) <= maxSize );

         const size_t newCount = mergeAndRemoveDuplicates( events, eventCount, cpuFreqGHz() );

         CoreEventData coresData;
         for ( uint32_t i = 0; i < newCount; ++i )
         {
            coresData.entries.ends.push_back( events[i].core.end );
            coresData.entries.starts.push_back( events[i].core.start );
            coresData.cores.push_back( events[i].core.core );
         }
         coresData.entries.depths.append( newCount, 0 );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.coreEventsPerThread[threadIndex].append( coresData );
         return ( size_t )( bufPtr - data );
      }
      case HOP_PROFILER_HEARTBEAT:
      {
         return ( size_t )( bufPtr - data );
      }
      default:
         assert( false );
         return ( size_t )( bufPtr - data );
   }
}

void Server::clearPendingMessages()
{
   size_t size = 0;
   while( hop_consume_shared_memory( _sharedMem, &size ) )
   {
      hop_release_shared_memory( _sharedMem, size );
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
      hop_set_listening_consumer( _sharedMem, false );
      hop_set_connected_consumer( _sharedMem, false );

      if( _thread.joinable() )
      {
         _thread.join();
      }
   }
}

void Server::PendingData::clear()
{
   HOP_ENTER_FUNC( 0 );

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

   HOP_LEAVE();
}

void Server::PendingData::swap( PendingData& rhs )
{
   HOP_ENTER_FUNC( 0 );
   using std::swap;
   swap( tracesPerThread, rhs.tracesPerThread );
   swap( stringData, rhs.stringData );
   swap( lockWaitsPerThread, rhs.lockWaitsPerThread );
   swap( unlockEventsPerThread, rhs.unlockEventsPerThread );
   swap( coreEventsPerThread, rhs.coreEventsPerThread );
   swap( threadNames, rhs.threadNames );
   HOP_LEAVE();
}

}  // namespace hop