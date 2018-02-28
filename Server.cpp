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
bool Server::start( const char* name )
{
   assert( name != nullptr );

   _running = true;

   _thread = std::thread( [this, name]() {
      while ( true )
      {
         // Try to get the shared memory
         if ( !_sharedMem.data() )
         {
            bool success = _sharedMem.create( name, HOP_SHARED_MEM_SIZE, true );
            if ( !success )
            {
               std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
               continue;
            }
            printf( "Connection to shared data succesful.\n" );
         }

         _sharedMem.waitSemaphore();

         // We are done running.s
         if ( !_running ) break;

         size_t offset = 0;
         const size_t bytesToRead = ringbuf_consume( _sharedMem.ringbuffer(), &offset );
         if ( bytesToRead > 0 )
         {
            size_t bytesRead = 0;
            while ( bytesRead < bytesToRead )
            {
               bytesRead += handleNewMessage(
                   &_sharedMem.data()[offset + bytesRead], bytesToRead - bytesRead );
            }
            ringbuf_release( _sharedMem.ringbuffer(), bytesToRead );
         }
      }
   } );

   return true;
}

void Server::setRecording( bool recording )
{
   _sharedMem.setListeningConsumer( recording );
}

size_t Server::handleNewMessage( uint8_t* data, size_t maxSize )
{
   uint8_t* bufPtr = data;
   const MsgInfo* msgInfo = (const MsgInfo*)bufPtr;
   const MsgType msgType = msgInfo->type;
   const uint32_t threadId = msgInfo->threadId;

    bufPtr += sizeof( MsgInfo );
    assert( (size_t)(bufPtr - data) <= maxSize );

    switch ( msgType )
    {
      case MsgType::PROFILER_TRACE:
      {
         // Copy string and add it to database
         std::vector< char > stringData( msgInfo->traces.stringDataSize );
         if( stringData.size() > 0 )
         {
            memcpy( stringData.data(), bufPtr, stringData.size() );
            stringDb.addStringData( stringData );
            bufPtr += stringData.size();
         }
         // If our string table is emtpy and we did not received any strings,
         // it means these messages are pending messages from previous execution.
         // Ignore them until we receive string data. This can happen after an
         // unclean exit.
         else if ( stringDb.empty() )
         {
            return maxSize;
         }
         assert( (size_t)(bufPtr - data) <= maxSize );

        const Trace* traces = (const Trace*) bufPtr;
        const size_t traceCount = msgInfo->traces.traceCount;

        DisplayableTraces dispTraces;
        dispTraces.reserve( traceCount );
        for( size_t i = 0; i < traceCount; ++i )
        {
            const auto& t = traces[i];
            dispTraces.ends.push_back( t.end );
            dispTraces.deltas.push_back( t.end - t.start );
            dispTraces.fileNameIds.push_back( stringDb.getStringIndex( t.fileNameId ) );
            dispTraces.classNameIds.push_back( stringDb.getStringIndex( t.classNameId ) );
            dispTraces.fctNameIds.push_back(  stringDb.getStringIndex( t.fctNameId ) );
            dispTraces.lineNbs.push_back( t.lineNumber );
            dispTraces.depths.push_back( t.depth );
            dispTraces.groups.push_back( t.group );
        }

        // The ends time should already be sorted
        assert_is_sorted( dispTraces.ends.begin(), dispTraces.ends.end() );

        bufPtr += ( traceCount * sizeof( Trace ) );
        assert( ( size_t )( bufPtr - data ) <= maxSize );

        // TODO: Could lock later when we received all the messages
        std::lock_guard<std::mutex> guard( pendingTracesMutex );
        pendingTraces.emplace_back( std::move( dispTraces ) );
        pendingStringData.emplace_back( std::move( stringData ) );
        pendingThreadIds.push_back( threadId );
        return ( size_t )( bufPtr - data );
      }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         std::vector< LockWait > lockwaits( msgInfo->lockwaits.count );
         memcpy( lockwaits.data(), bufPtr, lockwaits.size() * sizeof(LockWait) );

         bufPtr += lockwaits.size() * sizeof(LockWait);
         assert( (size_t)(bufPtr - data) <= maxSize );

         std::sort(
             lockwaits.begin(), lockwaits.end(), []( const LockWait& lhs, const LockWait& rhs ) {
                return lhs.start < rhs.start;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<std::mutex> guard( pendingLockWaitsMutex );
         pendingLockWaits.emplace_back( std::move( lockwaits ) );
         pendingLockWaitThreadIds.push_back( threadId );

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
         std::lock_guard<std::mutex> guard( pendingUnlockEventsMutex );
         pendingUnlockEvents.emplace_back( std::move( unlockEvents ) );
         pendingUnlockEventsThreadIds.push_back( threadId );
         return (size_t)(bufPtr - data);
      }
      default:
         assert( false );
         return (size_t)(bufPtr - data);
   }
}

void Server::getPendingProfilingTraces(
    std::vector< hop::DisplayableTraces >& tracesFrame,
    std::vector< std::vector< char > >& stringData,
    std::vector< uint32_t >& threadIds )
{
   std::lock_guard<std::mutex> guard( pendingTracesMutex );
   std::swap( tracesFrame, pendingTraces );
   std::swap( stringData, pendingStringData );
   std::swap( threadIds, pendingThreadIds );

   pendingTraces.clear();
   pendingStringData.clear();
   pendingThreadIds.clear();
}

void Server::getPendingLockWaits(
    std::vector<std::vector<LockWait> >& lockWaits,
    std::vector<uint32_t>& threadIds )
{
   std::lock_guard<std::mutex> guard( pendingLockWaitsMutex );
   std::swap( lockWaits, pendingLockWaits );
   std::swap( threadIds, pendingLockWaitThreadIds );

   pendingLockWaits.clear();
   pendingLockWaitThreadIds.clear();
}

void Server::getPendingUnlockEvents(
    std::vector<std::vector<UnlockEvent> >& unlockEvents,
    std::vector<uint32_t>& threadIds )
{
   std::lock_guard<std::mutex> guard( pendingUnlockEventsMutex );
   std::swap( unlockEvents, pendingUnlockEvents );
   std::swap( threadIds, pendingUnlockEventsThreadIds );

   pendingUnlockEvents.clear();
   pendingUnlockEventsThreadIds.clear();
}

void Server::stop()
{
   if( _running )
   {
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

}  // namespace hop