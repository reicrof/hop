#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <cstdint>

namespace hop
{

typedef int ProcessID;
struct ProcessInfo
{
   ProcessID pid;
   char name[64];
};

void cpuid( int reg[4], int fctId );

ProcessInfo getProcessInfoFromPID( ProcessID pid );
ProcessInfo getProcessInfoFromProcessName( const char* name );

ProcessID startChildProcess( const char* path, char** args );

}  // namespace hop

#endif