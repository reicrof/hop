#include <client.h>
#include <message.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vdbg
{

bool Client::connect( const char* serverName )
{
   _socket = socket( AF_UNIX, SOCK_STREAM, 0 );
   if ( _socket < 0 )
   {
      perror( "socket() failed" );
      return false;
   }

   struct sockaddr_un serveraddr;
   memset( &serveraddr, 0, sizeof( serveraddr ) );
   serveraddr.sun_family = AF_UNIX;
   strcpy( serveraddr.sun_path, serverName );

   int rc = ::connect( _socket, (struct sockaddr*)&serveraddr, SUN_LEN( &serveraddr ) );
   if ( rc < 0 )
   {
      //perror( "connect() failed" );
      return false;
   }

   return true;
}

bool Client::send( uint8_t* data, uint32_t size ) const
{
   int rc = ::send( _socket, data, size, 0 );
   if ( rc < 0 )
   {
      perror( "send() failed" );
      return false;
   }

   return true;
}

void Client::disconnect() { ::close( _socket ); }

} // namespace vdbg
