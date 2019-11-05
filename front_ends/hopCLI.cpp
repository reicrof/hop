#define HOP_IMPLEMENTATION
#include <Hop.h>
#include "platform/Platform.h"
#include "common/Profiler.h"
#include "common/Utils.h"
#undef main

#include "miniz.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <signal.h>
#include <string>
#include <thread>

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

static std::unique_ptr<hop::Profiler> createProfiler( const char* processName )
{
   using namespace hop;
   const int pid = getPIDFromString( processName );
   const hop::ProcessInfo procInfo = pid != -1 ? hop::getProcessInfoFromPID( pid )
                                               : hop::getProcessInfoFromProcessName( processName );

   auto profiler = std::make_unique<hop::Profiler>( Profiler::SRC_TYPE_PROCESS, procInfo.pid, processName );
   profiler->setRecording( true );
   return profiler;
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

static void printHelp()
{
   printf( "'help'   'h' \tShow this help menu\n"
           "'quit'   'q' \tStop recording, save data and exit\n"
           "'record' 's' \tStart recording\n"
           "'stop'   's' \tStop recording\n");
}

enum CommandType
{
   CMD_TYPE_INVALID,
   CMD_TYPE_HELP,
   CMD_TYPE_EXIT,
   CMD_TYPE_START_RECORDING,
   CMD_TYPE_STOP_RECORDING,
};

struct Command
{
   CommandType type;
};

struct StringCommand
{
   const char* cmdStr;
   CommandType type;
};

static constexpr StringCommand stringCmds[] =
{
   {"h", CMD_TYPE_HELP},
   {"help", CMD_TYPE_HELP},

   {"exit", CMD_TYPE_EXIT},
   {"q", CMD_TYPE_EXIT},
   {"quit", CMD_TYPE_EXIT},

   {"r", CMD_TYPE_START_RECORDING},
   {"record", CMD_TYPE_START_RECORDING},
   {"start", CMD_TYPE_STOP_RECORDING},

   {"s", CMD_TYPE_START_RECORDING},
   {"stop", CMD_TYPE_START_RECORDING},
};

std::mutex commandsMutex;
std::vector< Command > g_commands;
static Command parseCmdLine( std::string cmdline )
{
   Command cmd;
   cmd.type = CMD_TYPE_INVALID;

   const size_t end = std::string::npos;

   std::transform( cmdline.begin(), cmdline.end(), cmdline.begin(), ::tolower );
   for( auto it = std::begin( stringCmds ); it != std::end( stringCmds ); ++it )
   {
      if( strcmp( it->cmdStr, cmdline.c_str() ) == 0 )
      {
         cmd.type = it->type;
         break;
      }
   }

   return cmd;
}

static void interpretCmdline()
{
   std::string cmdline;
   while( g_run )
   {
      std::getline( std::cin, cmdline );

      const Command cmd = parseCmdLine( cmdline );
      if( cmd.type != CMD_TYPE_INVALID )
      {
         std::lock_guard<std::mutex> g( commandsMutex );
         g_commands.push_back( cmd );
      }
      else
      {
         printf( "Unknown command. Use 'help' to list available commands\n" );
      }

      std::cin.clear();
   }
}

static bool processCommands()
{
   std::vector< Command > localCmds;
   {
   std::lock_guard<std::mutex> g( commandsMutex );
   localCmds = std::move( g_commands );
   g_commands.clear();
   }

   for( const auto& cmd : localCmds )
   {
      switch (cmd.type)
      {
      case CMD_TYPE_EXIT:
         g_run = false;
         break;
      case CMD_TYPE_HELP:
         printHelp();
         break;
      case CMD_TYPE_START_RECORDING:
         break;
      case CMD_TYPE_STOP_RECORDING:
         break;
      default:
         assert( !"Invalid command" );
         break;
      }
   }

   return localCmds.size();
}

static void showPrompt()
{
   printf( "> " );
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

   std::unique_ptr<hop::Profiler> profiler;
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
      profiler = createProfiler( opts.processName );
   }

   // Start the command line interpreter
   g_commands.reserve( 32 );
   std::thread interpreter( interpretCmdline );
   interpreter.detach();

   showPrompt();
   while ( g_run )
   {
      HOP_PROF( "Main Loop" );

      if( showPrompt )

      profiler->fetchClientData();

      if( processCommands() )
      {
         showPrompt();
      }

      std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
   }

   // We have launched a child process. Let's close it
   if ( opts.startExec )
   {
      terminateProcess( childProcId );
   }

   // The interpreter thread will leak. This is a small cost to pay to have simple dumb portable code.
}
