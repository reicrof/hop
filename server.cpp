#include <server.h>

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <algorithm>
#include <chrono>

namespace vdbg
{
bool Server::start( const char* name, int  )
{
   assert( name != nullptr );

   _running = true;

   _thread = std::thread( [this, name]() {
      using namespace details;
      while ( _running )
      {

         // Try to get the shared memory
         if( !_sharedMem.data() )
         {
            bool success = _sharedMem.create( name, VDBG_SHARED_MEM_SIZE, true );
            if( !success )
            {
               continue;
            }
            printf("Connection to shared data succesful.\n");
         }

         sem_wait( _sharedMem.semaphore() );
         size_t offset = 0;
         const size_t bytesToRead = ringbuf_consume( _sharedMem.ringbuffer(), &offset );
         if( bytesToRead > 0 )
         {
            size_t bytesRead = 0;
            while( bytesRead < bytesToRead )
            {
               bytesRead += handleNewMessage( &_sharedMem.data()[offset + bytesRead], bytesToRead - bytesRead );
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
   using namespace details;

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
         std::vector< char > stringData( msgInfo->traces.stringDataSize );
         memcpy( stringData.data(), bufPtr, stringData.size() );

         bufPtr += stringData.size();
         assert( (size_t)(bufPtr - data) <= maxSize );

         const Trace* traces = (const Trace*) bufPtr;
         const size_t traceCount = msgInfo->traces.traceCount;
         //printf( "Profiler Trace from thread %u with %d traces received\n", threadId, msgInfo.traces.traceCount );
         std::vector< DisplayableTrace > dispTrace;
         dispTrace.reserve( traceCount * 2 );
         for( size_t i = 0; i < traceCount; ++i )
         {
            const Trace& t = traces[i];
            // TODO: hack! needs to taking into account the precision specified in message.h
            const uint32_t difference = (t.end - t.start);
            dispTrace.push_back( DisplayableTrace{
                t.start, difference, DisplayableTrace::START_TRACE, t.classNameIdx, t.fctNameIdx} );
            dispTrace.push_back( DisplayableTrace{
                t.end, difference, DisplayableTrace::END_TRACE, t.classNameIdx, t.fctNameIdx} );
         }

         bufPtr += ( traceCount * sizeof( Trace ) );
         assert( (size_t)(bufPtr - data) <= maxSize );

         // Sort them by time
         std::sort( dispTrace.begin(), dispTrace.end() );

         std::lock_guard<std::mutex> guard( pendingTracesMutex );
         pendingTraces.emplace_back( std::move( dispTrace ) );
         pendingStringData.emplace_back( std::move( stringData ) );
         pendingThreadIds.push_back( threadId );
         return (size_t)(bufPtr - data);
      }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         // std::vector< LockWait > lockwaits( msgInfo->lockwaits.count );
         // const size_t bytesToRead = msgInfo->lockwaits.count * sizeof( LockWait );
         // size_t valread = ::read( clientId, (void*)lockwaits.data(), bytesToRead );
         // if( valread != bytesToRead )
         //    return 0;

         //printf("Received %lu wait locks\n", lockwaits.size() );
         return (size_t)(bufPtr - data);
      }
      default:
         return (size_t)(bufPtr - data);
   }
   return (size_t)(bufPtr - data);
}

void Server::getPendingProfilingTraces(
    std::vector< std::vector< vdbg::DisplayableTrace > >& tracesFrame,
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

void Server::stop()
{
   if( _running )
   {
      _running = false;
      // Wake up semaphore to close properly
      sem_post( _sharedMem.semaphore() );
      if ( _thread.joinable() ) _thread.join();
   }
}

}  // namespace vdbg