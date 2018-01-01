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
                     unsigned char buffer[sizeof( vdbg::MsgHeader )];
                     size_t valread = read( i, buffer, sizeof( vdbg::MsgHeader ) );
                     if ( valread == 0 )
                     {
                        // Client disconnect
                        printf( "Client %d has disconnected.\n", i );
                        i = 0;
                     }
                     else
                     {
                        MsgHeader* header = reinterpret_cast<vdbg::MsgHeader*>( buffer );
                        handleNewMessage(
                            i, header->type, header->size - sizeof( vdbg::MsgHeader ) );
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

bool Server::handleNewMessage( int clientId, vdbg::MsgType type, uint32_t /*size*/ )
{
   switch ( type )
   {
      case MsgType::PROFILER_TRACE:
      {
         vdbg::TracesInfo info;
         size_t valread = ::read( clientId, (void*)&info, sizeof( info ) );

         if( valread != sizeof( info ) )
            return false;

         std::vector< char > stringData( info.stringDataSize );
         valread = ::read( clientId, stringData.data(), info.stringDataSize );
         if( valread != info.stringDataSize )
            return false;

         std::vector< vdbg::Trace > traces( info.traceCount );
         valread = ::read( clientId, (void*)traces.data(), sizeof( vdbg::Trace ) * info.traceCount );
         if( valread != sizeof( vdbg::Trace ) * info.traceCount )
            return false;

         //printf( "Profiler Trace from thread %u with %d traces received\n", info.threadId, info.traceCount );
         std::vector< vdbg::DisplayableTrace > dispTrace;
         dispTrace.reserve( info.traceCount * 2 );
         for( const auto& t : traces )
         {
            // TODO: hack! needs to taking into account the precision specified in message.h
            const float difference = (t.end - t.start) * 0.001;
            const double start =static_cast<double>(t.start);
            const double end = static_cast<double>(t.end);
            dispTrace.push_back( DisplayableTrace{ start, difference, 1, t.classNameIdx, t.fctNameIdx } );
            dispTrace.push_back( DisplayableTrace{ end, 0.0f, 0, 0, 0 } );
         }

         std::sort(
             dispTrace.begin(),
             dispTrace.end(),
             []( const DisplayableTrace& a, const DisplayableTrace& b ) -> bool {
                return a.time < b.time;
             } );

         std::lock_guard<std::mutex> guard( pendingTracesMutex );
         pendingTraces.emplace_back( std::move( dispTrace ) );
         pendingStringData.emplace_back( std::move( stringData ) );
         pendingThreadIds.push_back( info.threadId );
         return true;
      }
      default:
         return false;
   }
}

void Server::getProfilingTraces(
    std::vector< std::vector< vdbg::DisplayableTrace > >& tracesFrame,
    std::vector< std::vector< char > >& stringData,
    std::vector< uint32_t >& threadIds )
{
   std::lock_guard<std::mutex> guard( pendingTracesMutex );
   tracesFrame = std::move( pendingTraces );
   stringData = std::move( pendingStringData );
   threadIds = std::move( pendingThreadIds );

   pendingTraces.clear();
   pendingStringData.clear();
   threadIds.clear();
   // The stringData should not be cleared
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