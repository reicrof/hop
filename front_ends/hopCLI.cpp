#define HOP_IMPLEMENTATION
#include <Hop.h>
#include "common/Startup.h"
#include "common/Profiler.h"
#include "common/Utils.h"
#include "platform/Platform.h"
#undef main

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

std::atomic< bool > g_run{true};

static const std::string WHITESPACES = " \n\r\t\f\v";

enum CommandType
{
   CMD_TYPE_EMTPY,
   CMD_TYPE_INVALID,
   CMD_TYPE_HELP,
   CMD_TYPE_EXIT,
   CMD_TYPE_BEGIN_RECORDING,
   CMD_TYPE_END_RECORDING,
   CMD_TYPE_STATUS,
   CMD_TYPE_CLEAR
};

struct Command
{
   CommandType type;
};

struct StringCommand
{
   const char* cmdStr;
   const char* shortCmdStr;
   const char* description;
   CommandType type;
};

static constexpr StringCommand stringCmds[] =
{
   {"help", "h", "Show this help menu", CMD_TYPE_HELP},
   {"quit", "q", "Quit profiling, saving data", CMD_TYPE_EXIT},
   {"begin", "b", "Begin recording", CMD_TYPE_BEGIN_RECORDING},
   {"end", "e", "End recording", CMD_TYPE_END_RECORDING},
   {"status", "s", "Show status of the profiling", CMD_TYPE_STATUS},
   {"clear", "c", "Clear collected traces and stop recording", CMD_TYPE_CLEAR},
};

static void terminateCallback( int sig )
{
   g_run.store( false );
}

static std::string lefttrim(const std::string& s)
{
	size_t last = s.find_first_not_of( WHITESPACES );
	return ( last == std::string::npos ) ? "" : s.substr( last );
}

static std::string righttrim( const std::string& s )
{
	size_t first = s.find_last_not_of( WHITESPACES );
	return ( first == std::string::npos ) ? "" : s.substr(0, first + 1);
}

static std::unique_ptr<hop::Profiler> createProfiler( const char* processName, bool startRecording )
{
   using namespace hop;
   const int pid = getPIDFromString( processName );
   const hop::ProcessInfo procInfo = pid != -1 ? hop::getProcessInfoFromPID( pid )
                                               : hop::getProcessInfoFromProcessName( processName );

   auto profiler = std::make_unique<hop::Profiler>( Profiler::SRC_TYPE_PROCESS, procInfo.pid, processName );
   if( startRecording )
      profiler->setRecording( true );
   return profiler;
}

static int validateArguments( const hop::LaunchOptions& opts )
{
   if( !opts.processName )
   {
      fprintf( stderr, "No process to profile specified.\n" );
      return -1;
   }

   if( !opts.saveFilePath )
   {
      fprintf( stderr, "No output save path specified.\n" );
      return -1;
   }

   return 0;
}

static void printInvalidCmd()
{
   printf( "Unknown command. Use 'help' to list available commands\n" );
}

static void printHelp()
{
   const int arraySz = sizeof( stringCmds ) / sizeof( stringCmds[0] );
   for( int i = 0; i < arraySz; ++i )
   {
      printf(
          "%s\t%s : %s\n",
          stringCmds[i].cmdStr,
          stringCmds[i].shortCmdStr,
          stringCmds[i].description );
   }
}

static void printStatus( hop::Profiler* prof )
{
   int pid = -1;
   const char* name = prof->nameAndPID( &pid );
   const char* recordState = prof->recording() && pid != -1 ? "Recording" : "Not Recording";
   const hop::ProfilerStats stats = prof->stats();
   printf("%s (%d) - [%s] \n\tTraces Count : %zu\n", name, pid, recordState, stats.traceCount );
}

std::mutex commandsMutex;
std::vector< Command > g_commands;
static Command parseCmdLine( std::string cmdline )
{
   std::transform( cmdline.begin(), cmdline.end(), cmdline.begin(), ::tolower );
   cmdline = lefttrim( cmdline );
   cmdline = righttrim( cmdline );
   const char* lowerCmdLine = cmdline.c_str();

   Command cmd;

   // Return empty command
   if (cmdline.empty())
   {
      cmd.type = CMD_TYPE_EMTPY;
      return cmd;
   }

   // Otherwise assume it is invalid for now
   cmd.type = CMD_TYPE_INVALID;
   for( auto it = std::begin( stringCmds ); it != std::end( stringCmds ); ++it )
   {
      if( strcmp( it->shortCmdStr, lowerCmdLine ) == 0 ||
          strcmp( it->cmdStr, lowerCmdLine ) == 0 )
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
   while( g_run.load() )
   {
      std::getline( std::cin, cmdline );

      const Command cmd = parseCmdLine( cmdline );
      std::lock_guard<std::mutex> g( commandsMutex );
      g_commands.push_back( cmd );
   }
}

static bool processCommands( hop::Profiler* prof )
{
   std::vector< Command > localCmds;
   {
   std::lock_guard<std::mutex> g( commandsMutex );
   localCmds = std::move( g_commands );
   g_commands.clear();
   }

   bool shouldPrintStatus = false;
   for( const auto& cmd : localCmds )
   {
      switch (cmd.type)
      {
      case CMD_TYPE_EXIT:
         g_run.store( false );
         break;
      case CMD_TYPE_HELP:
         printHelp();
         break;
      case CMD_TYPE_BEGIN_RECORDING:
         prof->setRecording( true );
         shouldPrintStatus = true;
         break;
      case CMD_TYPE_END_RECORDING:
         prof->setRecording( false );
         shouldPrintStatus = true;
         break;
      case CMD_TYPE_STATUS:
         shouldPrintStatus = true;
         break;
      case CMD_TYPE_INVALID:
         printInvalidCmd();
         break;
      case CMD_TYPE_EMTPY:
         shouldPrintStatus = true;
         break;
      case CMD_TYPE_CLEAR:
         prof->clear();
         shouldPrintStatus = true;
         break;
      default:
         break;
      }
   }

   if( shouldPrintStatus )
      printStatus( prof );

   return localCmds.size();
}

static void showPrompt()
{
   printf( ">>> " );
   fflush( stdout );
}

int main( int argc, char* argv[] )
{
   // Confirm the platform supports HOP
   if ( !hop::verifyPlatform() )
   {
      return -2;
   }

   const hop::LaunchOptions opts = hop::parseArgs( argc, argv );
   if( const int err = validateArguments(opts) )
   {
      exit( err );
   }

   hop::setupSignalHandlers( terminateCallback );

   HOP_SET_THREAD_NAME( "Main" );

   std::unique_ptr<hop::Profiler> profiler;
   hop::ProcessID childProcId = 0;
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
   profiler = createProfiler( opts.processName, opts.startExec );

   // Start the command line interpreter
   g_commands.reserve( 32 );
   std::thread interpreter( interpretCmdline );
   interpreter.detach();

   showPrompt();
   while ( g_run.load() )
   {
      HOP_PROF( "Main Loop" );

      profiler->fetchClientData();
      if( processCommands( profiler.get() ) )
      {
         showPrompt();
      }

      std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
   }

   assert( opts.saveFilePath );

   profiler->saveToFile( opts.saveFilePath );

   // We have launched a child process. Let's close it
   if ( opts.startExec )
   {
      hop::terminateProcess( childProcId );
   }

   // The interpreter thread will leak. This is a small cost to pay to have simple dumb portable code.
}
