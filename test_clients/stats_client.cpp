#include <cstring>
#include <chrono>
#include <thread>

#include <time.h>
#include <signal.h>

#define HOP_IMPLEMENTATION
#include <Hop.h>

volatile bool g_run = true;

static std::atomic<int> workerId{0};
void sleepSome( std::chrono::microseconds sleepTime )
{
   HOP_PROF_FUNC();
   HOP_ZONE( HOP_ZONE_COLOR_1 );
   char name[256];
   snprintf( name, 256, "Worker %d", workerId.fetch_add(1) );
   HOP_SET_THREAD_NAME( name );

   thread_local int64_t  int64value = -250;
   thread_local uint64_t uint64value = 250;
   thread_local float    floatValue = 0.5f;

   HOP_STATS_INT64( "Int Value", ++int64value );
   HOP_STATS_FLOAT64( "Float Value", ++floatValue );

   std::this_thread::sleep_for( sleepTime );
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
         sleepSome( microseconds( 10000 ) );
         sleepSome( microseconds( 100 ) );
      } } );
   }

    HOP_SET_THREAD_NAME( "MAIN THREAD" );
    std::this_thread::sleep_for( microseconds( 2500 ) );
    while(g_run)
    {
       sleepSome( microseconds( 10000 ) );
       sleepSome( microseconds( 100 ) );
    }

    for( auto& t : threads )
    {
      t.join();
    }
}
