#include "Platform.h"

#include <intrin.h>
#include <Shlwapi.h>

namespace hop
{
void cpuid( int reg[4], int fctId ) { __cpuid( reg, fctId ); }

processId_t startChildProcess( const char* path, char** )
{
   processId_t newProcess = 0;
   STARTUPINFO si         = {0};
   PROCESS_INFORMATION pi = {0};

   // TODO Fix arguments passing
   si.cb = sizeof( si );
   if( !CreateProcess( NULL, (LPSTR)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) )
   {
      return (processId_t)-1;
   }
   newProcess = pi.hProcess;
   return newProcess;
}

const char* locateSubString( const char* haystack, const char* needle )
{
   return StrStrI( haystack, needle );
}

}  // namespace hop