#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <signal.h>

#define HOP_IMPLEMENTATION
#define HOP_CPP
#include <Hop.h>

class MyMutex
{
public:
   void lock()
   {
      HOP_ACQUIRE_LOCK( &m );
      m.lock();
      HOP_LOCK_ACQUIRED();
   }

   void unlock()
   {
      HOP_RELEASE_LOCK( &m );
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
   HOP_ENTER_FUNC( 0 );
   while( i > 0 )
   {
      std::lock_guard<MyMutex> g{mxs[i]};
      if( i%5 == 0 )
         std::this_thread::sleep_for(std::chrono::microseconds(1));
      rec(--i);
   }
   HOP_LEAVE();
}

void startRec()
{
   HOP_PROF_FUNC();
   HOP_ENTER_FUNC( 0 );
   int recCount = RECURSION_COUNT;
   rec( recCount );
   HOP_LEAVE();
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

   HOP_INTIALIZE();

   int threadNum = 1;
   if( argc > 1 )
      threadNum = atoi( argv[1] );

   HOP_ZONE(1);
   std::vector< std::thread > threads;
   for( int i = 0; i < threadNum; ++i )
      threads.emplace_back( [](){ while(g_run) { startRec(); } } );

   while( g_run )
   {
      startRec();
   }

   HOP_SHUTDOWN();
}