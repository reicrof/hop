//#include <client.h>
#include <cstring>
//#include <vdbg_client.h>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define VDBG_IMPLEMENTATION
#include <vdbg.h>

using namespace std::chrono_literals;

int bug = -1;

// static void stall()
// {
//    VDBG_PROF_FUNC();
//    std::this_thread::sleep_for(10ms);
// }

// static void buggyFunction()
// {
//    VDBG_PROF_FUNC();
//    int test = rand() % 100 + 1;
//    if( test == bug )
//    {
//       stall();
//    }
// }

// struct MaClasse
// {
//    void callBuggyFunction()
//    {
//       VDBG_PROF_MEMBER_FUNC();
//       buggyFunction();
//    }
// };

// static void func3()
// {
//    VDBG_PROF_FUNC();
//    std::this_thread::sleep_for(1ms);
// }
// static void func2()
// {
//    VDBG_PROF_FUNC();
//    func3();
//    buggyFunction();
//    func3();
// }
// static void func1()
// {
//    static size_t i = 0;
//    VDBG_PROF_FUNC();
//    func2();
//    ++i;
//    printf( "%lu\n", i );
// }

// static void threadFunc()
// {
//    while(true)
//    {
//       std::this_thread::sleep_for(1000ms);
//       func3();
//       func3();
//       func3();
//       func3();
//       func3();
//    }
// }

// static volatile size_t stamps = 0;
// static void paintStamp()
// {
//    VDBG_PROF_FUNC();
//    stamps++;
// }

// static void paint()
// {
//    for( size_t i = 0; i < 1000; ++i )
//    {
//       VDBG_PROF_FUNC();
//       paintStamp();
//       std::this_thread::sleep_for(0.001ms);
//    }
// }

static void recCall( int& v )
{
   VDBG_PROF_FUNC();
   // using namespace std::chrono_literals;
   // std::this_thread::sleep_for(0.01ms);
   if( v > 0 )
   {
      recCall( --v );
   }
}

// static void call1( int v )
// {
//    VDBG_PROF_FUNC();
//    recCall( v );
//    paint();
// }

#include <mutex>
int main()
{
   // srand (time(NULL));
   // bug = rand() % 100 + 1;

   //static std::thread t1( threadFunc );
   //std::mutex m;

   const auto preDrawTime = std::chrono::system_clock::now();

   static size_t asdf = 0;
   while(asdf < 100000)
   {
      VDBG_PROF_FUNC();
      ++asdf;
      int val = 2000;
      recCall( val );
      // using namespace std::chrono_literals;
      // std::this_thread::sleep_for(10ms);
      // func1();
      // MaClasse a;
      // a.callBuggyFunction();
      // if( asdf > 10 )
      // {
      //    std::lock_guard<std::mutex> g(m);
      //    call1(60);
      // }
   }

   const auto postDrawTime = std::chrono::system_clock::now();
   auto lastTime = std::chrono::duration< double, std::milli>( ( postDrawTime - preDrawTime ) ).count();

   printf("took %f\n", lastTime );

}
