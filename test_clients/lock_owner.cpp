#include <cstring>
#include <chrono>
#include <thread>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <time.h>
#include <signal.h>

#define HOP_IMPLEMENTATION
#include <Hop.h>

class MyMutex
{
public:
   void lock()
   {
      HOP_PROF_MUTEX_LOCK( &m );
      m.lock();
   }

   void unlock()
   {
      HOP_PROF_MUTEX_UNLOCK( &m );
      m.unlock();
   }

   std::mutex m;
};

bool g_run = true;
MyMutex g_mutex;

static std::atomic<int> workerId{0};
void testMutex()
{
   char name[256];
   snprintf( name, 256, "MUTEX WORKER %d", workerId.fetch_add(1) );
   HOP_SET_THREAD_NAME( name );
   HOP_PROF_FUNC();

   {
   std::lock_guard<MyMutex> g(g_mutex);
   HOP_PROF( "lockgaurd" );
   std::this_thread::sleep_for(std::chrono::microseconds(10000));
   }
}

#if !defined(_MSC_VER)
void terminateCallback(int sig)
{
   signal(sig, SIG_IGN);
   g_run = false;
}
#endif

int main( int argc, const char** argv )
{
#if !defined(_MSC_VER)
   // Setup signal handlers
   signal(SIGINT, terminateCallback);
   signal(SIGTERM, terminateCallback);
#endif

   int threadNum = 2;
   if( argc > 1 )
      threadNum = atoi( argv[1] );

   std::vector< std::thread > threads;
   for( int i = 0; i < threadNum; ++i )
      threads.emplace_back( [](){ while(g_run) { testMutex(); } } );

    HOP_SET_THREAD_NAME( "MAIN THREAD" );
    std::this_thread::sleep_for(std::chrono::microseconds(25000));
    while(g_run)
    {
      testMutex();
    }
}
