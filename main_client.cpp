#include <client.h>
#include <cstring>
#include <message.h>

int main()
{
   vdbg::Client client;
   client.connect( vdbg::SERVER_PATH );

   vdbg::Trace aTrace = {};
   aTrace.start = vdbg::getTimeStamp();
   aTrace.end = vdbg::getTimeStamp();
   strncpy( aTrace.name, __func__, vdbg::MAX_FCT_NAME_LENGTH-1 );
   vdbg::Trace tinfo[32] = {};

   tinfo[0] = aTrace;
   tinfo[15] = aTrace;
   tinfo[31] = aTrace;

   vdbg::TracesInfo info;
   info.threadId = 0;
   info.traceCount = 32;

   uint32_t msgSize = sizeof( vdbg::TracesInfo ) + 32 * sizeof( vdbg::Trace );
   uint8_t buffer[ sizeof( vdbg::MsgHeader ) + msgSize ];

   vdbg::MsgHeader h = { vdbg::MsgType::PROFILER_TRACE, msgSize };
   memcpy( buffer, &h, sizeof( h ) );
   memcpy( buffer+sizeof(h), &info, sizeof( info ) );
   memcpy( buffer+sizeof(h)+sizeof(info), tinfo, sizeof( tinfo ) );
   for(int i = 0; i < 1024; ++i )
      client.send( buffer, sizeof( buffer ) );
}