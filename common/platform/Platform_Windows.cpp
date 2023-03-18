#include "Platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <intrin.h>
#include <signal.h>
#include <Psapi.h>
#include <TlHelp32.h>

namespace hop
{
void cpuid( int reg[4], int fctId ) { __cpuid( reg, fctId ); }

ProcessID startChildProcess( const char* path, char** )
{
   ProcessID newProcess   = 0;
   STARTUPINFO si         = {0};
   PROCESS_INFORMATION pi = {0};

   // TODO Fix arguments passing
   si.cb = sizeof( si );
   if( !CreateProcess( NULL, (LPSTR)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) )
   {
      return (ProcessID)-1;
   }
   newProcess = (ProcessID)pi.hProcess;
   CloseHandle( pi.hThread );
   return newProcess;
}

ProcessInfo getProcessInfoFromPID( ProcessID pid )
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

ProcessesInfo getProcessInfoFromProcessName( const char* name )
{
   ProcessesInfo infos;
   infos.count = 0;

   HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );
   PROCESSENTRY32 entry;
   entry.dwSize = sizeof( PROCESSENTRY32 );
   if( Process32First( snapshot, &entry ) == TRUE )
   {
      while( Process32Next( snapshot, &entry ) == TRUE )
      {
         if( _stricmp( entry.szExeFile, name ) == 0 )
         {
            infos.infos[infos.count].pid = entry.th32ProcessID;
            strncpy(
                infos.infos[infos.count].name,
                entry.szExeFile,
                sizeof( infos.infos[infos.count].name ) - 1 );
            infos.count++;
         }
      }
   }

   CloseHandle( snapshot );

   return infos;
}

bool processAlive( hop::ProcessID id )
{
   DWORD exitCode;
   GetExitCodeProcess( (HANDLE)id, &exitCode );
   return exitCode == STILL_ACTIVE;
}

void terminateProcess( hop::ProcessID id )
{
   TerminateProcess( (HANDLE)id, 0 );
   WaitForSingleObject( (HANDLE)id, INFINITE );
   CloseHandle( (HANDLE)id );
}

void setupSignalHandlers( void (*terminateCB)(int) )
{
   // Setup signal handlers
   signal( SIGINT, terminateCB );
   signal( SIGTERM, terminateCB );
}

void* virtualAlloc( size_t size )
{
#ifdef HOP_DEBUG
   const LPVOID basePtr = (LPVOID)0x80000000LL;
#else
   const LPVOID basePtr = nullptr;
#endif
   
   return VirtualAlloc( basePtr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
}

void virtualFree( void* memory, uint64_t size )
{
   VirtualFree( memory, size, MEM_RELEASE );
}

uint32_t getTempFolderPath( char* buffer, uint32_t size )
{
   return GetTempPathA( size, buffer );
}

uint32_t getWorkingDirectory( char* buffer, uint32_t size )
{
   return GetCurrentDirectoryA( size, buffer );
}

}  // namespace hop