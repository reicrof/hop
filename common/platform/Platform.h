#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <cstdint>

namespace hop
{

typedef int64_t ProcessID;
struct ProcessInfo
{
   ProcessID pid;
   char name[256];
};
struct ProcessesInfo
{
   ProcessInfo infos[6];
   uint8_t count;
};

void cpuid( int reg[4], int fctId );

ProcessInfo getProcessInfoFromPID( ProcessID pid );
ProcessesInfo getProcessInfoFromProcessName( const char* name );

ProcessID startChildProcess( const char* path, char** args );

bool processAlive( hop::ProcessID id );
void terminateProcess( hop::ProcessID id );
void setupSignalHandlers( void (*terminateCB)(int) );

void* virtualAlloc( uint64_t size );
void virtualFree( void* memory, uint64_t size );

uint32_t getTempFolderPath( char* buffer, uint32_t size );
uint32_t getWorkingDirectory( char* buffer, uint32_t size );

}  // namespace hop

#endif
