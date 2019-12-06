#include "Platform.h"

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

namespace hop
{

void cpuid( int reg[4], int fctId )
{
   asm volatile( "cpuid"
                 : "=a"( reg[0] ), "=b"( reg[1] ), "=c"( reg[2] ), "=d"( reg[3] )
                 : "a"( fctId ), "c"( 0 ) );
}

ProcessInfo getProcessInfoFromPID( ProcessID pid )
{
   ProcessInfo info = {};

   char cmd[128] = {};
   snprintf( cmd, sizeof( cmd ), "ps -p %d -o comm= | xargs basename | tr -d '\n'", pid );

   // Get name from PID
   if( FILE* fp = popen( cmd, "r" ) )
   {
      if( fgets( info.name, sizeof( info.name ), fp ) != nullptr )
      {
         info.pid = pid;
      }
      pclose( fp );
   }

   return info;
}

ProcessInfo getProcessInfoFromProcessName( const char* name )
{
   ProcessInfo info = {};

   if( strlen( name ) > 0 )
   {
      // Get actual PID from process name
      char cmd[128] = {};

      /**
       * Trying to find a whole word match for the process name. We also need to remove the
       * grep process from the result. On MacOs the process name also contains the path, we
       * thus have to check for a preceding '/'
       */
      snprintf( cmd, sizeof( cmd ), "ps -A | grep '[ \\/]%s$' | grep -v grep | awk '{print $1}'", name );

      // Get name from PID
      if( FILE* fp = popen( cmd, "r" ) )
      {
         char pidStr[16] = {};
         if( fgets( pidStr, sizeof( pidStr ), fp ) != nullptr )
         {
            info.pid = strtol( pidStr, nullptr, 10 );
            strncpy( info.name, name, sizeof( info.name ) - 1 );
         }
         else
         {
            info.pid = -1;
         }
         pclose( fp );
      }
   }

   return info;
}

ProcessID startChildProcess( const char* path, char** args )
{
   ProcessID newProcess = 0;

   newProcess = (ProcessID) fork();
   if ( newProcess == 0 )
   {
      int res = execvp( path, args );
      if ( res < 0 )
      {
         exit( 0 );
      }
   }
   return newProcess;
}

bool processAlive( hop::ProcessID id )
{
   return kill( id, 0 ) == 0;
}

void terminateProcess( hop::ProcessID id )
{
   if ( processAlive( id ) )
   {
      kill( id, SIGTERM );
      int status, wpid;
      do
      {
         wpid = wait( &status );
      }
      while ( wpid > 0 );
   }
}

void setupSignalHandlers( void (*terminateCB)(int) )
{
   signal( SIGINT, terminateCB );
   signal( SIGTERM, terminateCB );
   signal( SIGCHLD, SIG_IGN );
}

uint32_t getTempFolderPath( char* buffer, uint32_t size )
{
   const char unixTempFolder[] = "/tmp/";
   if( size >= sizeof( unixTempFolder ) )
   {
      strcpy( buffer, unixTempFolder );
      return sizeof( unixTempFolder ) - 1; // We usually do not count the null byte
   }

   return 0;
}

} //  namespace hop