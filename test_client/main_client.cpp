#include <cstring>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <time.h>

#define HOP_IMPLEMENTATION
#include <Hop.h>

using namespace std::chrono_literals;

int bug = -1;
size_t count = 0;

void stall()
{
   HOP_PROF_FUNC();
   std::this_thread::sleep_for(100us);
}

void buggyFunction()
{
   HOP_PROF_FUNC();
   std::this_thread::sleep_for(250us);
   if( count % 20 == 0 )
   {
      for( int i =0; i < 100; ++i )
         stall();
   }
}

struct MaClasse
{
   void callBuggyFunction()
   {
      HOP_PROF_MEMBER_FUNC();
      buggyFunction();
   }
};

void func3()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   std::this_thread::sleep_for(500us);
}
void func2()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   func3();
   buggyFunction();
   func3();
}
void func1()
{
   static size_t i = 0;
   HOP_PROF_FUNC_WITH_GROUP(42);
   func2();
   ++i;
   printf( "%lu\n", i );
}

std::mutex m;

void doEvenMoreStuff()
{
   static size_t micros = 1;
   micros*=2;
   if( micros > 50000 )
   {
      micros = 1;
   }
   std::chrono::microseconds waittime(micros);
   HOP_PROF_FUNC_WITH_GROUP(42);
   std::this_thread::sleep_for(waittime);
}

void doMoreStuf()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   std::this_thread::sleep_for(25us);
   for( int i = 0; i < 200; ++i )
      doEvenMoreStuff();
}

void takeMutexAndDoStuff()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   std::lock_guard<std::mutex> g(m);
   {
      HOP_PROF_FUNC_WITH_GROUP(42);
      doMoreStuf();
      //std::this_thread::sleep_for(10us);
      std::this_thread::sleep_for(1ms);
      doMoreStuf();
   }
}

void threadFunc()
{
   while(true)
   {
      takeMutexAndDoStuff();
   }
}

void LODTest()
{
   std::this_thread::sleep_for(10ms);

   std::thread t1( threadFunc );
   std::thread t2( threadFunc );
   std::thread t3( threadFunc );
   std::thread t4( threadFunc );
   std::thread t5( threadFunc );
   std::thread t6( threadFunc );
   std::thread t7( threadFunc );
   std::thread t8( threadFunc );

   std::this_thread::sleep_for(10ms);

   while( true )
   {
      takeMutexAndDoStuff();
   }
}

int main()
{
   // srand (time(NULL));
   // bug = rand() % 100 + 1;

   //const auto preDrawTime = std::chrono::system_clock::now();

   std::thread t1 ( [](){ while( true ) { func1(); } } );
   std::thread t2 ( [](){ while( true ) { func1(); } } );
   std::thread t3 ( [](){ while( true ) { func1(); } } );

   while(true)
   {
      HOP_PROF_FUNC_WITH_GROUP(42);
      using namespace std::chrono_literals;
      std::lock_guard<std::mutex> g(m);
      std::this_thread::sleep_for(3ms);
      func1();
      MaClasse a;
      a.callBuggyFunction();
      ++count;
   }

   // const auto postDrawTime = std::chrono::system_clock::now();
   // auto lastTime = std::chrono::duration< double, std::milli>( ( postDrawTime - preDrawTime ) ).count();

   // printf("took %f\n", lastTime );

}
