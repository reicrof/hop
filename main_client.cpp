#include <client.h>
#include <message.h>
#include <cstring>

int main()
{
   unsigned char buffer[4096];
   memset( buffer, 'a', sizeof( buffer ) );

   vdbg::Client client;
   client.connect( vdbg::SERVER_PATH );

   vdbg::MsgHeader h = { vdbg::MsgType::MSG_1, sizeof( buffer ) };
   memcpy( buffer, &h, sizeof( h ) );
   client.send( buffer, sizeof( buffer ) );
}