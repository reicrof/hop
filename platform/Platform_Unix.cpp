#include "Platform.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

namespace hop
{

void cpuid( int reg[4], int fctId )
{
   asm volatile( "cpuid"
                 : "=a"( reg[0] ), "=b"( reg[1] ), "=c"( reg[2] ), "=d"( reg[3] )
                 : "a"( fctId ), "c"( 0 ) );
}

ProcessInfo getProcessInfoFromPID( processId_t pid )
{
   ProcessInfo info = {0, 0};

   char cmd[128] = {};
   snprintf( cmd, sizeof( cmd ), "basename `ps -p %d -o comm=`", pid );

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
   ProcessInfo info = {0, 0};

   // Get actual PID from process name
   char cmd[128] = {};
   snprintf( cmd, sizeof( cmd ), "pgrep %s", name );

   // Get name from PID
   if( FILE* fp = popen( cmd, "r" ) )
   {
      char pidStr[16]{};
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

   return info;
}

processId_t startChildProcess( const char* path, char** args )
{
   processId_t newProcess = 0;

   newProcess = (processId_t) fork();
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

const char* locateSubString( const char* haystack, const char* needle )
{
   return strcasestr( haystack, needle );
}

} //  namespace hop