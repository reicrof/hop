#include <vdbg_client.h>
#include <cassert>

#include <cstdio>

namespace vdbg
{
namespace details
{
size_t ClientProfiler::threadsId[MAX_THREAD_NB] = {0};
ClientProfiler::Impl* ClientProfiler::clientProfilers[MAX_THREAD_NB] = {0};

class ClientProfiler::Impl
{
public:
   void addProfilingTrace(
       const char* className,
       const char* fctName,
       vdbg::TimeStamp start,
       vdbg::TimeStamp end,
       uint8_t group )
   {
      printf( "Adding trace %s::%s %lu in %d \n", className ? className : "", fctName, end-start, group );
   }

   int pushTraceLevel{0};
};

ClientProfiler::Impl* ClientProfiler::Get( std::thread::id tId )
{
   const size_t threadIdHash = std::hash<std::thread::id>{}( tId );

   int tIndex = 0;
   int firstEmptyIdx = -1;
   for ( ; tIndex < MAX_THREAD_NB; ++tIndex )
   {
      // Find existing client profiler for current thread.
      if ( threadsId[tIndex] == threadIdHash ) break;
      if ( firstEmptyIdx == -1 && threadsId[tIndex] == 0 ) firstEmptyIdx = tIndex;
   }

   // If we have not found any existing client profiler for the current thread,
   // lets create one.
   if ( tIndex >= MAX_THREAD_NB )
   {
      assert( firstEmptyIdx < MAX_THREAD_NB );
      // TODO: fix leak and fix potential condition race... -_-
      tIndex = firstEmptyIdx;
      threadsId[tIndex] = threadIdHash;
      clientProfilers[tIndex] = new ClientProfiler::Impl;
   }

   return clientProfilers[tIndex];
}

void ClientProfiler::StartProfile( ClientProfiler::Impl* impl )
{
   ++impl->pushTraceLevel;
}

void ClientProfiler::EndProfile(
    ClientProfiler::Impl* impl,
    const char* name,
    const char* classStr,
    vdbg::TimeStamp start,
    vdbg::TimeStamp end,
    uint8_t group )
{
   const int remainingPushedTraces = --impl->pushTraceLevel;
   impl->addProfilingTrace( classStr, name, start, end, group );
   if( remainingPushedTraces <= 0 )
   {
   }
}

}  // namespace details
}  // namespace vdbg
