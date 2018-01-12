//#include <client.h>
#include <cstring>
//#include <vdbg_client.h>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <time.h>

#define VDBG_IMPLEMENTATION
#include <vdbg.h>

using namespace std::chrono_literals;

// int bug = -1;

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

std::mutex m;

static void takeMutexAndDoStuff()
{
   VDBG_PROF_FUNC();
   std::lock_guard<std::mutex> g(m);
   {
      VDBG_PROF_FUNC();
      std::this_thread::sleep_for(10ms);
   }
}

static void threadFunc()
{
   while(true)
   {
      takeMutexAndDoStuff();
   }
}

int main()
{
   // srand (time(NULL));
   // bug = rand() % 100 + 1;

   std::this_thread::sleep_for(10ms);

   std::thread t1( threadFunc );

   std::this_thread::sleep_for(10ms);

   while( true )
   {
      takeMutexAndDoStuff();
   }

   // const auto preDrawTime = std::chrono::system_clock::now();

   // while(true)
   // {
   //    VDBG_PROF_FUNC();
   //    using namespace std::chrono_literals;
   //    std::lock_guard<std::mutex> g(m);
   //    std::this_thread::sleep_for(10ms);
   //    func1();
   //    MaClasse a;
   //    a.callBuggyFunction();
   // }

   // const auto postDrawTime = std::chrono::system_clock::now();
   // auto lastTime = std::chrono::duration< double, std::milli>( ( postDrawTime - preDrawTime ) ).count();

   // printf("took %f\n", lastTime );

}
