#ifndef HOP_LAUNCH_ARGS_H_
#define HOP_LAUNCH_ARGS_H_

#include <stdint.h>

namespace hop
{
static constexpr uint64_t VIRT_MEM_BLK_SIZE = 4 * 1024 * 1024 * 1024ULL;

struct LaunchOptions
{
   const char* fullProcessPath;
   const char* processName;
   const char* saveFilePath;
   char** args;
   bool startExec;
};

void printUsage( const char* progname );
LaunchOptions parseArgs( int argc, char* argv[] );

// Returns true if the current platform supports RDTSC and Invariant TSC
bool verifyPlatform();

}

#endif //  HOP_LAUNCH_ARGS_H_
