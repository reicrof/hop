#include <client.h>
#include <cstring>
#include <vdbg_client.h>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std::chrono_literals;

int bug = -1;

static void stall()
{
   VDBG_PROF_FUNC();
   std::this_thread::sleep_for(10ms);
}

static void buggyFunction()
{
   VDBG_PROF_FUNC();
   int test = rand() % 100 + 1;
   if( test == bug )
   {
      stall();
   }
}

static void func3()
{
   VDBG_PROF_FUNC();
   std::this_thread::sleep_for(1ms);
}
static void func2()
{
   VDBG_PROF_FUNC();
   func3();
   buggyFunction();
   func3();
}
static void func1()
{
   static size_t i = 0;
   VDBG_PROF_FUNC();
   func2();
   ++i;
   printf( "%lu\n", i );
}

static void threadFunc()
{
   while(true)
   {
      std::this_thread::sleep_for(1000ms);
      func3();
      func3();
      func3();
      func3();
      func3();
   }
}

int main()
{
   vdbg::Client client;
   client.connect( vdbg::SERVER_PATH );

   srand (time(NULL));
   bug = rand() % 100 + 1;

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
      VDBG_PROF_FUNC();
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1ms);
      func1();
   }

}
