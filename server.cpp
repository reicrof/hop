#include <server.h>

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <chrono>

namespace vdbg
{
bool Server::start( const char* name, int connections )
{
   assert( name != nullptr );
   _serverName = std::string( name );
   _clients.resize( connections, 0 );

   _socket = socket( AF_UNIX, SOCK_STREAM, 0 );
   if ( _socket < 0 )
   {
      perror( "socket() failed" );
      return false;
   }

   struct sockaddr_un serveraddr = {};
   serveraddr.sun_family = AF_UNIX;
   strcpy( serveraddr.sun_path, name );

   remove( name );
   int rc = bind( _socket, (struct sockaddr*)&serveraddr, SUN_LEN( &serveraddr ) );
   if ( rc < 0 )
   {
      perror( "bind() failed" );
      return false;
   }

   rc = listen( _socket, 1 );
   if ( rc < 0 )
   {
      perror( "listen() failed" );
      return -1;
   }

   _running = true;

   _thread = std::thread( [this]() {
      using namespace details;
      int max_sd = 0;
      struct timeval timeout = {0, 500000};
      while ( _running )
      {
         // clear the socket set
         FD_ZERO( &_fdSet );

         // add master socket to set
         FD_SET( _socket, &_fdSet );
         max_sd = _socket;

         for ( auto i : _clients )
         {
            if ( i > 0 ) FD_SET( i, &_fdSet );
            if ( i > max_sd ) max_sd = i;
         }

         // wait for an activity on one of the sockets with a timeout
         int activity = select( max_sd + 1, &_fdSet, NULL, NULL, &timeout );

         if ( activity >= 0 )
         {
            if ( FD_ISSET( _socket, &_fdSet ) )
            {
               bool hasNewConnection = handleNewConnection();
               if ( hasNewConnection )
               {
                  printf( "New Connection!\n" );
               }
            }
            else
            {
               for ( auto& i : _clients )
               {
                  if ( FD_ISSET( i, &_fdSet ) )
                  {
                     unsigned char buffer[sizeof( MsgHeader )];
                     size_t valread = read( i, buffer, sizeof( MsgHeader ) );
                     if ( valread == 0 )
                     {
                        // Client disconnect
                        printf( "Client %d has disconnected.\n", i );
                        i = 0;
                     }
                     else
                     {
                        MsgHeader* header = reinterpret_cast<MsgHeader*>( buffer );
                        for( uint32_t i = 0; i < header->msgCount; ++i )
                        {
                           handleNewMessage( i, header->threadId );
                        }
                     }
                  }
               }
            }
         }
      }
   } );

   return true;
}

bool Server::handleNewConnection()
{
   int connection = accept( _socket, NULL, NULL );
   if ( connection < 0 )
   {
      perror( "accept() failed" );
      return false;
   }

   for ( auto& i : _clients )
   {
      if ( i == 0 )
      {
         i = connection;
         return true;
      }
   }

   return false;
}

bool Server::handleNewMessage( int clientId, uint32_t threadId )
{
   using namespace details;
   MsgInfo msgInfo;
   size_t valread = ::read( clientId, (void*)&msgInfo, sizeof( MsgType ) );
   if( valread != sizeof( MsgInfo ) )
      return false;

   switch ( msgInfo.type )
   {
      case MsgType::PROFILER_TRACE:
      {
         std::vector< char > stringData( msgInfo.traces.stringDataSize );
         valread = ::read( clientId, stringData.data(), msgInfo.traces.stringDataSize );
         if( valread != msgInfo.traces.stringDataSize )
            return false;

         std::vector< Trace > traces( msgInfo.traces.traceCount );
         valread = ::read( clientId, (void*)traces.data(), sizeof( Trace ) * msgInfo.traces.traceCount );
         if( valread != sizeof( Trace ) * msgInfo.traces.traceCount )
            return false;

         //printf( "Profiler Trace from thread %u with %d traces received\n", threadId, msgInfo.traces.traceCount );
         std::vector< DisplayableTrace > dispTrace;
         dispTrace.reserve( msgInfo.traces.traceCount * 2 );
         for( const auto& t : traces )
         {
            // TODO: hack! needs to taking into account the precision specified in message.h
            const float difference = (t.end - t.start) * 0.001;
            const double start =static_cast<double>(t.start);
            const double end = static_cast<double>(t.end);
            dispTrace.push_back( DisplayableTrace{
                start, difference, DisplayableTrace::START_TRACE, t.classNameIdx, t.fctNameIdx} );
            dispTrace.push_back( DisplayableTrace{
                end, difference, DisplayableTrace::END_TRACE, t.classNameIdx, t.fctNameIdx} );
         }

         // Sort them by time
         std::sort( dispTrace.begin(), dispTrace.end() );

         std::lock_guard<std::mutex> guard( pendingTracesMutex );
         pendingTraces.emplace_back( std::move( dispTrace ) );
         pendingStringData.emplace_back( std::move( stringData ) );
         pendingThreadIds.push_back( threadId );
         return true;
      }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         std::vector< LockWait > lockwaits( msgInfo.lockwaits.count );
         const size_t bytesToRead = msgInfo.lockwaits.count * sizeof( LockWait );
         size_t valread = ::read( clientId, (void*)lockwaits.data(), bytesToRead );
         if( valread != bytesToRead )
            return false;

         //printf("Received %lu wait locks\n", lockwaits.size() );
         return true;
      }
      default:
         return false;
   }
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

   close( _socket );
   unlink( _serverName.c_str() );
   _serverName.clear();
   FD_ZERO( &_fdSet );
   _clients.clear();
   _socket = -1;
}

}  // namespace vdbg