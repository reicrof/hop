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
   _sharedMemory.create( name, VDBG_SHARED_MEM_SIZE );

   _running = true;

   _thread = std::thread( [this]() {
      using namespace details;
      static size_t msgCount = 0;
      while ( _running )
      {
         sem_wait( _sharedMemory._semaphore );
         size_t offset = 0;
         size_t size = ringbuf_consume( _sharedMemory.ringbuffer(), &offset );
         if( size > 0 )
         {
            printf( "Woken up %lu! with : %s \n", ++msgCount, (char*)&_sharedMemory.data()[offset] );
         }
         ringbuf_release( _sharedMemory.ringbuffer(), size );
      }
   } );

   return true;
}

bool Server::handleNewMessage( int , uint32_t  )
{
   // using namespace details;
   // MsgInfo msgInfo;
   // size_t valread = ::read( clientId, (void*)&msgInfo, sizeof( MsgInfo ) );
   // if( valread != sizeof( MsgInfo ) )
   //    return false;

   // switch ( msgInfo.type )
   // {
   //    case MsgInfoType::PROFILER_TRACE:
   //    {
   //       std::vector< char > stringData( msgInfo.traces.stringDataSize );
   //       valread = ::read( clientId, stringData.data(), msgInfo.traces.stringDataSize );
   //       if( valread != msgInfo.traces.stringDataSize )
   //          return false;

   //       std::vector< Trace > traces( msgInfo.traces.traceCount );
   //       valread = ::read( clientId, (void*)traces.data(), sizeof( Trace ) * msgInfo.traces.traceCount );
   //       if( valread != sizeof( Trace ) * msgInfo.traces.traceCount )
   //          return false;

   //       //printf( "Profiler Trace from thread %u with %d traces received\n", threadId, msgInfo.traces.traceCount );
   //       std::vector< DisplayableTrace > dispTrace;
   //       dispTrace.reserve( msgInfo.traces.traceCount * 2 );
   //       for( const auto& t : traces )
   //       {
   //          // TODO: hack! needs to taking into account the precision specified in message.h
   //          const uint32_t difference = (t.end - t.start);
   //          dispTrace.push_back( DisplayableTrace{
   //              t.start, difference, DisplayableTrace::START_TRACE, t.classNameIdx, t.fctNameIdx} );
   //          dispTrace.push_back( DisplayableTrace{
   //              t.end, difference, DisplayableTrace::END_TRACE, t.classNameIdx, t.fctNameIdx} );
   //       }

   //       // Sort them by time
   //       std::sort( dispTrace.begin(), dispTrace.end() );

   //       std::lock_guard<std::mutex> guard( pendingTracesMutex );
   //       pendingTraces.emplace_back( std::move( dispTrace ) );
   //       pendingStringData.emplace_back( std::move( stringData ) );
   //       pendingThreadIds.push_back( threadId );
   //       return true;
   //    }
   //    case MsgInfoType::PROFILER_WAIT_LOCK:
   //    {
   //       std::vector< LockWait > lockwaits( msgInfo.lockwaits.count );
   //       const size_t bytesToRead = msgInfo.lockwaits.count * sizeof( LockWait );
   //       size_t valread = ::read( clientId, (void*)lockwaits.data(), bytesToRead );
   //       if( valread != bytesToRead )
   //          return false;

   //       //printf("Received %lu wait locks\n", lockwaits.size() );
   //       return true;
   //    }
   //    default:
   //       return false;
   // }
   return true;
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
   _running = false;
   if ( _thread.joinable() ) _thread.join();
}

}  // namespace vdbg