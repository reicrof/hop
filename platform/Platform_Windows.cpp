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

ProcessInfo getProcessInfoFromPID( processId_t pid )
{
   ProcessInfo info = {};

   HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid );
   if( NULL != hProcess )
   {
      HMODULE hMod;
      DWORD cbNeeded;
      if( EnumProcessModules( hProcess, &hMod, sizeof( hMod ), &cbNeeded ) )
      {
         if( GetModuleBaseName( hProcess, hMod, (LPSTR)info.name, sizeof( info.name ) ) > 0 )
         {
            info.pid = pid;
         }
      }
   }

   return info;
}

ProcessInfo getProcessInfoFromProcessName( const char* name )
{
   ProcessInfo info = {};

   // Create toolhelp snapshot.
   HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
   PROCESSENTRY32 process;
   ZeroMemory( &process, sizeof( process ) );
   process.dwSize = sizeof( process );

   // Walkthrough all processes.
   if( Process32First( snapshot, &process ) )
   {
      do
      {
         // Compare process.szExeFile based on format of name, i.e., trim file path
         // trim .exe if necessary, etc.
         if( strcmp( (const char*)process.szExeFile, name ) == 0 )
         {
            info.pid = process.th32ProcessID;
            strncpy( info.name, name, sizeof( info.name ) - 1 );
            break;
         }
      } while( Process32Next( snapshot, &process ) );
   }

   CloseHandle( snapshot );
   return info;
}

const char* locateSubString( const char* haystack, const char* needle )
{
   return StrStrI( haystack, needle );
}

}  // namespace hop