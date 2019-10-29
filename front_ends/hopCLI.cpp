#define HOP_IMPLEMENTATION
#include <Hop.h>
#include "platform/Platform.h"
//#include "Viewer.h"
#include "common/Utils.h"
#undef main

#include "miniz.h"

#include <signal.h>
#include <string>

#ifndef _MSC_VER
#include <sys/wait.h>
#endif

bool g_run = true;

static void terminateCallback( int sig )
{
   signal( sig, SIG_IGN );
   g_run = false;
}

static bool processAlive( hop::ProcessID id )
{
#if defined( _MSC_VER )
   DWORD exitCode;
   GetExitCodeProcess( (HANDLE)id, &exitCode );
   return exitCode == STILL_ACTIVE;
#else
   return kill( id, 0 ) == 0;
#endif
}

static void terminateProcess( hop::ProcessID id )
{
#if defined( _MSC_VER )
   TerminateProcess( (HANDLE)id, 0 );
   WaitForSingleObject( (HANDLE)id, INFINITE );
   CloseHandle( (HANDLE)id );
#else
   if ( processAlive( id ) )
   {
      kill( id, SIGINT );
      int status, wpid;
      do
      {
         wpid = wait( &status );
      }
      while ( wpid > 0 );
   }
#endif
}

static bool verifyPlatform()
{
   if ( !hop::supportsRDTSCP() )
   {
      printf(
          "This platform does not seem to support RDTSCP. Hop will not be "
          "able to work properly.\n" );
      return false;
   }

   if ( !hop::supportsConstantTSC() )
   {
      printf(
          "This platform does not seem to support Invariant TSC. Hop will be "
          "able to run, but no precision on the measurement are guaranteed.\n" );
   }
   return true;
}

static void printUsage()
{
   printf(
       "Usage : hop [OPTION] <process name>\n\n OPTIONS:\n\t-e Launch specified executable and "
       "start recording\n\t-v Display version info and exit\n\t-h Show usage\n" );
   exit( 0 );
}

struct LaunchOptions
{
   const char* fullProcessPath;
   const char* processName;
   char** args;
   bool startExec;
};

static LaunchOptions createLaunchOptions( char* fullProcessPath, char** argv, bool startExec )
{
   LaunchOptions opts = {fullProcessPath, fullProcessPath, argv, startExec};
   std::string fullPathStr( fullProcessPath );
   size_t lastSeparator = fullPathStr.find_last_of( "/\\" );
   if ( lastSeparator != std::string::npos )
   {
      opts.processName = &fullProcessPath[++lastSeparator];
   }

   return opts;
}

static LaunchOptions parseArgs( int argc, char* argv[] )
{
   if ( argc > 1 )
   {
      if ( argv[1][0] == '-' )
      {
         switch ( argv[1][1] )
         {
            case 'v':
               printf( "hop version %.2f \n", HOP_VERSION );
               exit( 0 );
               break;
            case 'h':
               printUsage();
               exit( 0 );
            case 'e':
               if ( argc > 2 )
               {
                  return createLaunchOptions( argv[2], &argv[2], true );
               }
               // Fallthrough
            default:
               fprintf( stderr, "Invalid arguments\n" );
               break;
         }
      }
      else
      {
         return createLaunchOptions( argv[1], &argv[1], false );
      }
   }
   return LaunchOptions{nullptr, nullptr, nullptr, false};
}

int main( int argc, char* argv[] )
{
   const LaunchOptions opts = parseArgs( argc, argv );

   // Setup signal handlers
   signal( SIGINT, terminateCallback );
   signal( SIGTERM, terminateCallback );
#ifndef _MSC_VER
   signal( SIGCHLD, SIG_IGN );
#endif

   // Confirm the platform can use HOP
   if ( !verifyPlatform() )
   {
      return -2;
   }

   HOP_SET_THREAD_NAME( "Main" );

   //hop::Viewer viewer( DM.w, DM.h );

   hop::ProcessID childProcId = 0;
   if ( opts.processName )
   {
      // If we want to launch an executable to profile, now is the time to do it
      if ( opts.startExec )
      {
         // profiler->setRecording( true );
         childProcId = hop::startChildProcess( opts.fullProcessPath, opts.args );
         if ( childProcId == (hop::ProcessID)-1 )
         {
            fprintf( stderr, "Could not launch child process\n" );
            exit( -1 );
         }
      }

      // Add new profiler after having potentially started it.
      //viewer.addNewProfiler( opts.processName, opts.startExec );
   }

   while ( g_run )
   {
      HOP_PROF( "Main Loop" );
      const auto frameStart = std::chrono::system_clock::now();

      const auto startFetch = std::chrono::system_clock::now();
      //viewer.fetchClientsData();
      const auto endFetch = std::chrono::system_clock::now();

      //viewer.onNewFrame( w, h, x, y, lmb, rmb, g_mouseWheel );

      //viewer.draw( w, h );

      const auto frameEnd = std::chrono::system_clock::now();
   }

   // We have launched a child process. Let's close it
   if ( opts.startExec )
   {
      terminateProcess( childProcId );
   }
}
