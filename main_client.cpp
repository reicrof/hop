#include <client.h>
#include <cstring>
#include <message.h>

int main()
{
   unsigned char buffer[4096];
   memset( buffer, 'a', sizeof( buffer ) );

   vdbg::Client client;
   client.connect( vdbg::SERVER_PATH );

   vdbg::TraceInfo info;
   info.start = vdbg::getTimeStamp();
   info.end = vdbg::getTimeStamp();
   strncpy( info.name, __func__, vdbg::MAX_FCT_NAME_LENGTH );
   vdbg::TraceInfo tinfo[32] = {};
   vdbg::Traces traces = {};

   tinfo[0] = info;
   tinfo[15] = info;
   tinfo[31] = info;

   uint32_t size = sizeof( traces ) + sizeof( tinfo );
   vdbg::MsgHeader h = { vdbg::MsgType::PROFILER_TRACE, size };
   memcpy( buffer, &h, sizeof( h ) );
   client.send( buffer, sizeof( buffer ) );
}