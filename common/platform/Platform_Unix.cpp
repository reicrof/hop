#include "Platform.h"

#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/*
   The comm= and cmd parameter passed to the 'o' argument of the grep acts differently
   on Linux and on MacOs. Since we want to have the full name of the executable we need
   to use these 2 versions for each platform
*/
#ifdef __APPLE__
#define HOP_GREP_CMD "comm="
// By default the xargs does not complai non Mac
#define HOP_XARGS_NO_RUN_EMPTY ""
#else
#define HOP_GREP_CMD "cmd"
// On linnux xargs complains if it receives and empty string
#define HOP_XARGS_NO_RUN_EMPTY "-r"
#endif

namespace hop
{

void cpuid( int reg[4], int fctId )
{
#ifdef __x86_64__
   asm volatile( "cpuid"
                 : "=a"( reg[0] ), "=b"( reg[1] ), "=c"( reg[2] ), "=d"( reg[3] )
                 : "a"( fctId ), "c"( 0 ) );
#else
   (void)fctId;
   memset( reg, 0, sizeof( int ) * 4 );
#endif
}

ProcessInfo getProcessInfoFromPID( ProcessID pid )
{
   ProcessInfo info = {};

   char cmd[128] = {};
   snprintf( cmd, sizeof( cmd ), "ps -p %" PRId64 " -o comm= | xargs " HOP_XARGS_NO_RUN_EMPTY " basename | tr -d '\n'", pid );

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

static inline bool isSpace (const char c)
{
   return c == ' ' || c == '\n';
}

ProcessesInfo getProcessInfoFromProcessName( const char* name )
{
   ProcessesInfo infos;
   infos.count = 0;

   if( strlen( name ) > 0 )
   {
      // Get actual PID from process name
      char cmd[128] = {};

      /**
       * Trying to find a whole word match for the process name. We also need to remove the
       * grep process from the result. On MacOs the process name also contains the path, we
       * thus have to check for a preceding '/'
       */
      snprintf( cmd, sizeof( cmd ), "ps -Ao pid," HOP_GREP_CMD " | grep -E '[0-9]+ \\.?\\/?\\S*\\/?%s\\b($| .*)'" , name );
      // Get name from PID
      if( FILE* fp = popen( cmd, "r" ) )
      {
         const int max_count = sizeof( infos.infos ) / sizeof( infos.infos[0] );
         int count = 0;
         char line[256];
         while (fgets(line, sizeof(line), fp) && count < max_count)
         {
            /* fgets keep the newline so remove it */
            size_t length = strlen( line );
            if (line[length-1] == '\n')
               line[length-1] = '\0';

            char* end;
            infos.infos[count].pid = strtol( line, &end, 10 );
            while (isSpace( *end ) && *end != '\0')
               ++end;
            const char *actual_name = *end != '\0' ? end : name;
            strncpy( infos.infos[count].name, actual_name, sizeof( infos.infos[count].name ) - 1 );
            count++;
         }
         infos.count = count;
         pclose( fp );
      }
   }

   return infos;
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

void* virtualAlloc( uint64_t size )
{
#ifdef HOP_DEBUG
   void* basePtr = (void*)0x80000000LL;
#else
   void* basePtr = nullptr;
#endif
   void* mem = mmap( basePtr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
   if( mem == (void *)-1 )
   {
      mem = nullptr;
   }
   return mem;
}

void virtualFree( void* memory, uint64_t size )
{
   munmap( memory, size );
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

uint32_t getWorkingDirectory( char* buffer, uint32_t size )
{
   if( getcwd( buffer, size ) )
      return strlen( buffer );
   return 0;
}

} //  namespace hop
