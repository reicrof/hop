#ifndef HOP_LAUNCH_ARGS_H_
#define HOP_LAUNCH_ARGS_H_

namespace hop
{

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