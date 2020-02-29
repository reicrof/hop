#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <cstdint>

namespace hop
{

typedef int64_t ProcessID;
struct ProcessInfo
{
   ProcessID pid;
   char name[64];
};

void cpuid( int reg[4], int fctId );

ProcessInfo getProcessInfoFromPID( ProcessID pid );
ProcessInfo getProcessInfoFromProcessName( const char* name );

ProcessID startChildProcess( const char* path, char** args );

bool processAlive( hop::ProcessID id );
void terminateProcess( hop::ProcessID id );
void setupSignalHandlers( void (*terminateCB)(int) );

uint32_t getTempFolderPath( char* buffer, uint32_t size );
uint32_t getWorkingDirectory( char* buffer, uint32_t size );

}  // namespace hop

#endif