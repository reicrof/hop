#include "Server.h"
#include "Utils.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <algorithm>
#include <chrono>

namespace hop
{
bool Server::start( const char* name, bool useGlFinishByDefault )
{
   assert( name != nullptr );

   _running = true;
   _connectionState = SharedMemory::NOT_CONNECTED;

   _thread = std::thread( [this, name, useGlFinishByDefault]() {
      while ( true )
      {
         // Try to get the shared memory
         if ( !_sharedMem.data() )
         {
            SharedMemory::ConnectionState state = _sharedMem.create( name, 0 /*will be define in shared metadata*/, true );
            if ( state != SharedMemory::CONNECTED )
            {
               _connectionState = state;
               if (!_running) return; // We are done without even opening the shared mem :(
               std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
               continue;
            }
            // Clear any remaining messages from previous execution now
            clearPendingMessages();
            setUseGlFinish( useGlFinishByDefault );
            _connectionState = state;
            printf( "Connection to shared data successful.\n" );
         }

         // Wait for a signal with a timeout of 1s (1000ms)
         const bool wasSignaled = _sharedMem.waitSemaphore( 3000 );
         _connectionState = wasSignaled ? SharedMemory::CONNECTED_NO_CLIENT : SharedMemory::CONNECTED;

         // We are done running.
         if ( !_running.load() ) break;

         if( !wasSignaled )
         {
            // We timed out.
            // If the profiled app has stopped, we need to signal the shared memory we are still
            // listening in case it connects back
            if( !_sharedMem.hasConnectedProducer() )
            {
               _sharedMem.setConnectedConsumer( true );
               _sharedMem.setListeningConsumer( true );
            }

            // Otherwise, it simply means the app is either not very productive or is being debuged
            continue;
         }

         // A clear was requested so we need to clear our string database
         if( _clearingRequested.load() )
         {
            _stringDb.clear();
            clearPendingMessages();
            _clearingRequested.store( false );
            _sharedMem.setResetTimestamp( getTimeStamp() );
            continue;
         }

         HOP_PROF( "Handle messages" );
         size_t offset = 0;
         const size_t bytesToRead = ringbuf_consume( _sharedMem.ringbuffer(), &offset );
         if ( bytesToRead > 0 )
         {
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

bool Server::setRecording( bool recording )
{
    bool stateChanged = false;
    if (_sharedMem.data())
    {
        _sharedMem.setListeningConsumer(recording);
        stateChanged = true;
    }
    return stateChanged;
}

void Server::getPendingData(PendingData & data)
{
    HOP_PROF_FUNC();
    std::lock_guard<std::mutex> guard(_pendingData.mutex);
    _pendingData.swap(data);
    _pendingData.clear();
}

size_t Server::handleNewMessage( uint8_t* data, size_t maxSize, TimeStamp minTimestamp )
{
   uint8_t* bufPtr = data;
   const MsgInfo* msgInfo = (const MsgInfo*)bufPtr;
   const MsgType msgType = msgInfo->type;
   const uint32_t threadIndex = msgInfo->threadIndex;

    bufPtr += sizeof( MsgInfo );
    assert( (size_t)(bufPtr - data) <= maxSize );

    // If the message was sent prior to the last reset timestamp, ignore it
    if( msgInfo->timeStamp < minTimestamp )
       return (size_t)(bufPtr - data);

    switch ( msgType )
    {
       case MsgType::PROFILER_STRING_DATA:
       {
          // Copy string and add it to database
          std::vector<char> stringData( msgInfo->stringData.size );
          if ( stringData.size() > 0 )
          {
             memcpy( stringData.data(), bufPtr, stringData.size() );
             _stringDb.addStringData( stringData );
             bufPtr += stringData.size();

             assert( ( size_t )( bufPtr - data ) <= maxSize );

             // TODO: Could lock later when we received all the messages
             std::lock_guard<std::mutex> guard( _pendingData.mutex );
             _pendingData.stringData.emplace_back( std::move( stringData ) );
          }
          return ( size_t )( bufPtr - data );
       }
       case MsgType::PROFILER_TRACE:
       {
          const Trace* traces = (const Trace*)bufPtr;
          const size_t traceCount = msgInfo->traces.count;

          DisplayableTraces dispTraces;
          TDepth_t maxDepth = 0;
          for ( size_t i = 0; i < traceCount; ++i )
          {
             const auto& t = traces[i];
             dispTraces.ends.push_back( t.end );
             dispTraces.deltas.push_back( t.end - t.start );
             dispTraces.fileNameIds.push_back( _stringDb.getStringIndex( t.fileNameId ) );
             dispTraces.fctNameIds.push_back( _stringDb.getStringIndex( t.fctNameId ) );
             dispTraces.lineNbs.push_back( t.lineNumber );
             dispTraces.depths.push_back( t.depth );
             dispTraces.zones.push_back( t.zone );
             maxDepth = std::max( maxDepth, t.depth );
          }
          dispTraces.maxDepth = maxDepth;

          // The ends time should already be sorted
          assert_is_sorted( dispTraces.ends.begin(), dispTraces.ends.end() );

          bufPtr += ( traceCount * sizeof( Trace ) );
          assert( ( size_t )( bufPtr - data ) <= maxSize );

          static_assert(
              std::is_move_constructible<DisplayableTraces>::value,
              "Displayble Traces not moveable" );
          if ( traceCount > 0 )
          {
             // TODO: Could lock later when we received all the messages
             std::lock_guard<std::mutex> guard( _pendingData.mutex );
             _pendingData.traces.emplace_back( std::move( dispTraces ) );
             _pendingData.tracesThreadIndex.push_back( threadIndex );
          }
          return ( size_t )( bufPtr - data );
      }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         const LockWait* lws = (const LockWait*)bufPtr;
         const uint32_t lwCount = msgInfo->lockwaits.count;

         DisplayableLockWaits dispLw;
         for( uint32_t i = 0; i < lwCount; ++i )
         {
            dispLw.ends.push_back( lws[i].end );
            dispLw.deltas.push_back( lws[i].end - lws[i].start );
            dispLw.depths.push_back( lws[i].depth );
            dispLw.mutexAddrs.push_back( lws[i].mutexAddress );
         }

         bufPtr += ( lwCount * sizeof( LockWait ) );

         // The ends time should already be sorted
         assert_is_sorted( dispLw.ends.begin(), dispLw.ends.end() );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<std::mutex> guard( _pendingData.mutex );
         _pendingData.lockWaits.emplace_back( std::move( dispLw ) );
         _pendingData.lockWaitThreadIndex.push_back( threadIndex );

         return (size_t)(bufPtr - data);
      }
      case MsgType::PROFILER_UNLOCK_EVENT:
      {
         std::vector< UnlockEvent > unlockEvents( msgInfo->unlockEvents.count );
         memcpy( unlockEvents.data(), bufPtr, unlockEvents.size() * sizeof(UnlockEvent) );

         bufPtr += unlockEvents.size() * sizeof(UnlockEvent);
         assert( (size_t)(bufPtr - data) <= maxSize );

         std::sort(
             unlockEvents.begin(), unlockEvents.end(), []( const UnlockEvent& lhs, const UnlockEvent& rhs ) {
                return lhs.time < rhs.time;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<std::mutex> guard(_pendingData.mutex);
         _pendingData.unlockEvents.emplace_back( std::move( unlockEvents ) );
         _pendingData.unlockEventsThreadIndex.push_back(threadIndex);
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
      if( _sharedMem.data() )
      {
         _sharedMem.setListeningConsumer( false );
         _sharedMem.setConnectedConsumer( false );
      }
      _running = false;
      // Wake up semaphore to close properly
      if( _sharedMem.data() && _sharedMem.semaphore() )
      {
         _sharedMem.signalSemaphore();
      }
      if ( _thread.joinable() )
      {
         _thread.join();
      }
   }
}

bool Server::useGlFinish() const noexcept
{
   return _sharedMem.isUsingGlFinish();
}

void Server::setUseGlFinish( bool useGlFinish )
{
   _sharedMem.setUseGlFinish( useGlFinish );
}

void Server::PendingData::clear()
{
    traces.clear();
    stringData.clear();
    tracesThreadIndex.clear();
    lockWaits.clear();
    lockWaitThreadIndex.clear();
    unlockEvents.clear();
    unlockEventsThreadIndex.clear();
}

void Server::PendingData::swap(PendingData & rhs)
{
    HOP_PROF_FUNC();
    using std::swap;
    swap(traces, rhs.traces);
    swap(stringData, rhs.stringData);
    swap(tracesThreadIndex, rhs.tracesThreadIndex);
    swap(lockWaits, rhs.lockWaits);
    swap(lockWaitThreadIndex, rhs.lockWaitThreadIndex);
    swap(unlockEvents, rhs.unlockEvents);
    swap(unlockEventsThreadIndex, rhs.unlockEventsThreadIndex);
}

}  // namespace hop