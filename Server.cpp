#include "Server.h"
#include "Utils.h"
#include "Stats.h"

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
   while( ++first != last )
   {
      if( !pred( *writePos, *first ) )
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
bool Server::start( const char* name )
{
   assert( name != nullptr );

   _running = true;
   _connectionState = SharedMemory::NOT_CONNECTED;

   _thread = std::thread( [this, name]() {
      TimeStamp lastSignalTime = getTimeStamp();
      SharedMemory::ConnectionState localState = SharedMemory::NOT_CONNECTED;

      char serverName[64];
      snprintf( serverName, sizeof( serverName ), "%s [Producer]", name );
      HOP_SET_THREAD_NAME( serverName );
      while ( true )
      {
         HOP_PROF_FUNC();

         // Try to get the shared memory
         if ( !_sharedMem.valid() )
         {
            SharedMemory::ConnectionState state = _sharedMem.create( name, 0 /*will be define in shared metadata*/, true );
            if ( state != SharedMemory::CONNECTED )
            {
               _connectionState = state;
               localState = state;
               if (!_running) return; // We are done without even opening the shared mem :(
               std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
               continue;
            }
            // Clear any remaining messages from previous execution now
            clearPendingMessages();
            _sharedMem.setListeningConsumer( _recording );
            g_stats.clientSharedMemSize = _sharedMem.sharedMetaInfo()->requestedSize;
            _connectionState = state;
            printf( "Connection to shared data successful.\n" );
         }

         const bool wasSignaled = _sharedMem.tryWaitSemaphore();

         // Check if we are done running.
         if ( !_running.load() ) break;

         // Check if its been a while since we have been signaleds
         const TimeStamp curTime = getTimeStamp();
         const bool producerLost = !_sharedMem.hasConnectedProducer() || curTime - lastSignalTime > 3000000000 ;
         const auto newState = producerLost ? SharedMemory::CONNECTED_NO_CLIENT : SharedMemory::CONNECTED;

         // Update state if it has changed
         if( localState != newState )
         {
            localState = newState;
            _connectionState.store( newState );
         }

         // A clear was requested so we need to clear our string database
         if( _clearingRequested.load() )
         {
            _stringDb.clear();
            clearPendingMessages();
            _clearingRequested.store( false );
            _sharedMem.setResetTimestamp( getTimeStamp() );
            _threadNamesReceived.clear();
            continue;
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
                   &_sharedMem.data()[offset + bytesRead], bytesToRead - bytesRead, minTimestamp);
            }
            ringbuf_release( _sharedMem.ringbuffer(), bytesToRead );
         }
      }
   } );

   return true;
}

SharedMemory::ConnectionState Server::connectionState() const
{
   return _connectionState.load();
}

void Server::setRecording( bool recording )
{
   _recording = recording;
   if ( _sharedMem.valid() )
   {
      _sharedMem.setListeningConsumer( recording );
   }
}

void Server::getPendingData(PendingData & data)
{
    HOP_PROF_FUNC();
    std::lock_guard<hop::Mutex> guard(_sharedPendingDataMutex);
    _sharedPendingData.swap(data);
    _sharedPendingData.clear();
}

bool Server::addUniqueThreadName( uint32_t threadIndex, StrPtr_t name )
{
   bool newInsert = false;
   if( _threadNamesReceived.size() <= threadIndex )
   {
      _threadNamesReceived.resize( threadIndex + 1, 0 );
   }

   if( _threadNamesReceived[ threadIndex ] == 0 )
   {
      _threadNamesReceived[ threadIndex ] = name;
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
    assert( (size_t)(bufPtr - data) <= maxSize );
    (void)maxSize; // Removed unused warning

    // If the message was sent prior to the last reset timestamp, ignore it
    if( msgInfo->timeStamp < minTimestamp )
       return (size_t)(bufPtr - data);

    // If the thread has an assigned name
    if( msgInfo->threadName != 0 && addUniqueThreadName( threadIndex, msgInfo->threadName ) )
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
             _stringDb.addStringData( strDataPtr, strSize );
             bufPtr += strSize;
             assert( ( size_t )( bufPtr - data ) <= maxSize );

             // TODO: Could lock later when we received all the messages
             std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
             _sharedPendingData.stringData.insert(
                 _sharedPendingData.stringData.end(), strDataPtr, strDataPtr + strSize );
          }
          return ( size_t )( bufPtr - data );
       }
       case MsgType::PROFILER_TRACE:
       {
          const Trace* traces = (const Trace*)bufPtr;
          const size_t traceCount = msgInfo->traces.count;

          TraceData traceData;
          Depth_t maxDepth = 0;
          for ( size_t i = 0; i < traceCount; ++i )
          {
             const auto& t = traces[i];
             traceData.entries.ends.push_back( t.end );
             traceData.entries.deltas.push_back( t.end - t.start );
             traceData.fileNameIds.push_back( _stringDb.getStringIndex( t.fileNameId ) );
             traceData.fctNameIds.push_back( _stringDb.getStringIndex( t.fctNameId ) );
             traceData.lineNbs.push_back( t.lineNumber );
             traceData.entries.depths.push_back( t.depth );
             traceData.zones.push_back( t.zone );
             maxDepth = std::max( maxDepth, t.depth );
          }
          traceData.entries.maxDepth = maxDepth;

          // The ends time should already be sorted
          assert_is_sorted( traceData.entries.ends.begin(), traceData.entries.ends.end() );

          bufPtr += ( traceCount * sizeof( Trace ) );
          assert( ( size_t )( bufPtr - data ) <= maxSize );

          static_assert(
              std::is_move_constructible<TraceData>::value,
              "Trace Data not moveable" );
          if ( traceCount > 0 )
          {
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
         for( uint32_t i = 0; i < lwCount; ++i )
         {
            lockwaitData.entries.ends.push_back( lws[i].end );
            lockwaitData.entries.deltas.push_back( lws[i].end - lws[i].start );
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

         return (size_t)(bufPtr - data);
      }
      case MsgType::PROFILER_UNLOCK_EVENT:
      {
         const size_t eventCount = msgInfo->unlockEvents.count;
         UnlockEvent* eventPtr = (UnlockEvent*)bufPtr;

         bufPtr += eventCount * sizeof(UnlockEvent);
         assert( (size_t)(bufPtr - data) <= maxSize );

         std::sort(
             eventPtr, eventPtr + eventCount, []( const UnlockEvent& lhs, const UnlockEvent& rhs ) {
                return lhs.time < rhs.time;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard(_sharedPendingDataMutex);
         auto& unlocks = _sharedPendingData.unlockEventsPerThread[threadIndex];
         unlocks.insert( unlocks.end(), eventPtr, eventPtr + eventCount );

         return (size_t)(bufPtr - data);
      }
      case MsgType::PROFILER_HEARTBEAT:
      {
         return (size_t)(bufPtr - data);
      }
      case MsgType::PROFILER_CORE_EVENT:
      {
         const size_t eventCount = msgInfo->coreEvents.count;
         CoreEvent* coreEventsPtr = (CoreEvent*) bufPtr;

         // Must be done before removing duplicates
         bufPtr += eventCount * sizeof(CoreEvent);
         assert( (size_t)(bufPtr - data) <= maxSize );

         const size_t newCount = mergeAndRemoveDuplicates( coreEventsPtr, eventCount );

         assert_is_sorted( coreEventsPtr, coreEventsPtr + newCount );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard(_sharedPendingDataMutex);
         auto& coreEvents = _sharedPendingData.coreEventsPerThread[threadIndex];
         coreEvents.insert( coreEvents.end(), coreEventsPtr, coreEventsPtr + newCount );
        return (size_t)(bufPtr - data);
      }
      default:
         assert( false );
         return (size_t)(bufPtr - data);
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
   _clearingRequested.store( true );
}

void Server::stop()
{
   if( _running )
   {
      if( _sharedMem.valid() )
      {
         _sharedMem.setListeningConsumer( false );
         _sharedMem.setConnectedConsumer( false );
      }
      _running = false;
      // Wake up semaphore to close properly
      if( _sharedMem.valid() && _sharedMem.semaphore() )
      {
         _sharedMem.signalSemaphore();
      }
      if ( _thread.joinable() )
      {
         _thread.join();
      }
   }
}

void Server::PendingData::clear()
{
    HOP_PROF_FUNC();

    stringData.clear();
    for( auto& traces : tracesPerThread )
    {
       traces.second.clear();
    }

    for( auto& lockwaits : lockWaitsPerThread )
    {
       lockwaits.second.clear();
    }

    for( auto& unlockEvents : unlockEventsPerThread )
    {
       unlockEvents.second.clear();
    }

    for( auto& coreEvents : coreEventsPerThread )
    {
       coreEvents.second.clear();
    }

    threadNames.clear();
}

void Server::PendingData::swap(PendingData & rhs)
{
    HOP_PROF_FUNC();
    using std::swap;
    swap(tracesPerThread, rhs.tracesPerThread);
    swap(stringData, rhs.stringData);
    swap(lockWaitsPerThread, rhs.lockWaitsPerThread);
    swap(unlockEventsPerThread, rhs.unlockEventsPerThread);
    swap(coreEventsPerThread, rhs.coreEventsPerThread);
    swap(threadNames, rhs.threadNames);
}

}  // namespace hop