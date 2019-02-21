#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
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

MyMutex g_mutex;

bool g_run = true;

static const int RECURSION_COUNT = 20;
thread_local std::vector< MyMutex > mxs( RECURSION_COUNT + 1 );

void rec( int& i )
{
   HOP_PROF_FUNC();
   while( i > 0 )
   {
      std::lock_guard<MyMutex> g{mxs[i]};
      if( i%5 == 0 )
         std::this_thread::sleep_for(std::chrono::microseconds(1));
      rec(--i);
   }
}

void startRec()
{
   HOP_SET_THREAD_NAME( "Test thread" );
   HOP_PROF_FUNC();
   int recCount = RECURSION_COUNT;
   rec( recCount );
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

   int threadNum = 1;
   if( argc > 1 )
      threadNum = atoi( argv[1] );

   std::vector< std::thread > threads;
   for( int i = 0; i < threadNum; ++i )
      threads.emplace_back( [](){ while(g_run) { startRec(); } } );

   while( g_run )
   {
      startRec();
   }
}