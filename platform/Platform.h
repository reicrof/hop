#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <cstdint>

namespace hop
{

typedef int processId_t;
struct ProcessInfo
{
   processId_t pid;
   char name[64];
};

void cpuid( int reg[4], int fctId );

ProcessInfo getProcessInfoFromPID( processId_t pid );
ProcessInfo getProcessInfoFromProcessName( const char* name );

processId_t startChildProcess( const char* path, char** args );

}  // namespace hop

#endif