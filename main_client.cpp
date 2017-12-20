#include <client.h>
#include <cstring>
#include <vdbg_client.h>
#include <chrono>
#include <thread>

static void func3()
{
   VDBG_PROF_FUNC();
}
static void func2()
{
   VDBG_PROF_FUNC();
   func3();
}
static void func1()
{
   VDBG_PROF_FUNC();
   func2();
}

static void threadFunc()
{
   using namespace std::chrono_literals;
   std::this_thread::sleep_for(1s);
   while(true)
   {
      std::this_thread::sleep_for(500ms);
      func1();
   }
}


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

   constexpr uint32_t msgSize = sizeof( vdbg::TracesInfo ) + 32 * sizeof( vdbg::Trace );
   uint8_t buffer[ sizeof( vdbg::MsgHeader ) + msgSize ];

   vdbg::MsgHeader h = { vdbg::MsgType::PROFILER_TRACE, msgSize };
   memcpy( buffer, &h, sizeof( h ) );
   memcpy( buffer+sizeof(h), &info, sizeof( info ) );
   memcpy( buffer+sizeof(h)+sizeof(info), tinfo, sizeof( tinfo ) );

   std::thread t1( threadFunc );

   while(true)
   {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(200ms);
      func1();
   }

}
