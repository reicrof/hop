#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

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

void rec( int& i )
{
   HOP_PROF_FUNC();
   while( i > 0 )
   {
      //std::lock_guard<MyMutex> g{g_mutex};
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      rec(--i);
   }
}

void startRec()
{
   HOP_PROF_FUNC();
   int recCount = 50;
   rec( recCount );
}

void terminateCallback(int sig)
{
   signal(sig, SIG_IGN);
   g_run = false;
}

int main( int argc, const char** argv )
{
   // Setup signal handlers
   signal(SIGINT, terminateCallback);
   signal(SIGTERM, terminateCallback);

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