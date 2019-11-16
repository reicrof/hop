#include <cstring>
#include <chrono>
#include <thread>
#include <string>

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <time.h>
#include <thread>
#include <signal.h>
#include <vector>

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
MyMutex g_mutex0;
MyMutex g_mutex1;

static std::atomic<int> workerId{0};
void testMutex( MyMutex& m, int mtxId, std::chrono::microseconds sleepTime )
{
   HOP_ZONE( HOP_ZONE_COLOR_1 );
   char name[256];
   snprintf( name, 256, "MUTEX WORKER %d", workerId.fetch_add(1) );
   HOP_SET_THREAD_NAME( name );
   
   char profGuardName[64];
   snprintf( profGuardName, sizeof( profGuardName ), "LockGuard for Mutex %d", mtxId );
   HOP_PROF_DYN_NAME( &profGuardName[13] );

   {
   std::lock_guard<MyMutex> g(m);
   HOP_PROF_DYN_NAME( profGuardName );
   std::this_thread::sleep_for( sleepTime );
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

   using namespace std::chrono;

   int threadNum = 2;
   if( argc > 1 )
      threadNum = atoi( argv[1] );

   std::vector< std::thread > threads;
   for (int i = 0; i < threadNum; ++i)
   {
      threads.emplace_back( []() { while (g_run) {
         testMutex( g_mutex1, 1, microseconds( 10000 ) );
         testMutex( g_mutex0, 0, microseconds( 100 ) );
      } } );
   }

    HOP_SET_THREAD_NAME( "MAIN THREAD" );
    std::this_thread::sleep_for( microseconds( 2500 ) );
    while(g_run)
    {
       testMutex( g_mutex0, 0, microseconds( 10000 ) );
       testMutex( g_mutex1, 1, microseconds( 100 ) );
    }

    for( auto& t : threads )
    {
      t.join();
    }
}
