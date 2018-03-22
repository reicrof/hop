#include <cstring>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <time.h>

#define HOP_IMPLEMENTATION
#include <Hop.h>

int bug = -1;
size_t count = 0;
//std::mutex m;
std::mutex m1;

void stall()
{
   HOP_PROF_FUNC();
}

void buggyFunction()
{
   HOP_PROF_FUNC();
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
      HOP_PROF("MaClass::callBuggyFunction");
      buggyFunction();
   }
};

void func3()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   std::this_thread::sleep_for(std::chrono::microseconds(500));
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
   //static size_t i = 0;
   HOP_PROF_FUNC_WITH_GROUP(42);
   //std::lock_guard<std::mutex> g(m1);
   func2();
   //++i;
   //printf( "%lu\n", i );
}

void doEvenMoreStuff()
{
   static size_t micros = 1;
   micros*=2;
   if( micros > 50000 )
   {
      micros = 1;
   }
}

void doMoreStuf()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   for( int i = 0; i < 200; ++i )
      doEvenMoreStuff();
}

void takeMutexAndDoStuff()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   //std::lock_guard<std::mutex> g(m);
   {
      HOP_PROF_FUNC_WITH_GROUP(42);
      doMoreStuf();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
   //std::this_thread::sleep_for(10ms);

   std::thread t1( threadFunc );
   std::thread t2( threadFunc );
   std::thread t3( threadFunc );
   std::thread t4( threadFunc );
   std::thread t5( threadFunc );
   std::thread t6( threadFunc );
   std::thread t7( threadFunc );
   std::thread t8( threadFunc );

   //std::this_thread::sleep_for(10ms);

   while( true )
   {
      takeMutexAndDoStuff();
   }
}

void l3()
{
	HOP_PROF_FUNC_WITH_GROUP(42);
	//std::this_thread::sleep_for(100us);
}

void l2()
{
	HOP_PROF_FUNC_WITH_GROUP(42);
	//std::this_thread::sleep_for(500us);
	for (int i = 0; i < 2; ++i)
	{
		l3();
	}
}

void l1()
{
	HOP_PROF_FUNC_WITH_GROUP(42);
	l2();
	l2();
}

void rec( int& i )
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   while( i > 0 )
   {
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      rec(--i);
   }
   

}

void startRec()
{
   HOP_PROF_FUNC_WITH_GROUP(42);
   int recCount = 50;
   rec( recCount );
}

void testMutex()
{
   HOP_PROF_FUNC_WITH_GROUP(42);

   const auto start = hop::getTimeStamp();
   std::lock_guard<std::mutex> g(m1);
   hop::ClientManager::EndLockWait( &m1, start, hop::getTimeStamp() );
   std::this_thread::sleep_for(std::chrono::microseconds(1000));
   HOP_PROF_MUTEX_UNLOCK( &m1 );
}

int main()
{
   // srand (time(NULL));
   // bug = rand() % 100 + 1;

   //const auto preDrawTime = std::chrono_literals::system_clock::now();

    std::thread t1 ( [](){ while( true ) { func1(); } } );
    std::thread t2 ( [](){ while( true ) { func1(); } } );
    std::thread t3 ( [](){ while( true ) { func1(); } } );

    while(true)
    {
       HOP_PROF_FUNC_WITH_GROUP(42);
       //std::lock_guard<std::mutex> g(m);
       std::this_thread::sleep_for(std::chrono::milliseconds(3));
       func1();
       {
       HOP_PROF_GL_FINISH( "GL finish" );
       HOP_PROF( "Creating maclass1" );
       {
          std::this_thread::sleep_for(std::chrono::microseconds(250));
          HOP_PROF( "Creating maclass2" );
          {
             std::this_thread::sleep_for(std::chrono::microseconds(250));
             HOP_PROF( "Creating MaClasselass3" );
             {
                std::this_thread::sleep_for(std::chrono::microseconds(250));
                HOP_PROF( "Creating maclass4" );
                {
                   std::this_thread::sleep_for(std::chrono::microseconds(250));
                   HOP_PROF( "Creating maclass5" );
                   {
                      //std::lock_guard<std::mutex> g(m1);
                      std::this_thread::sleep_for(std::chrono::microseconds(250));
                      startRec();
                      startRec();
                      startRec();
                      HOP_PROF( "Creating maclass6" );

                   }
                }
             }
          }
       }
       MaClasse a;
       a.callBuggyFunction();
       }
       ++count;
	   l1();
	   l1();
    }

   // std::thread t ( [](){ while( true ) { testMutex(); } } );
   // //std::thread t1 ( [](){ while( true ) { testMutex(); } } );
   // std::this_thread::sleep_for(std::chrono::microseconds(25000));
   // while( true )
   // {
   //   testMutex();
   // }

   // const auto postDrawTime = std::chrono::system_clock::now();
   // auto lastTime = std::chrono::duration< double, std::milli>( ( postDrawTime - preDrawTime ) ).count();

   // printf("took %f\n", lastTime );

}
