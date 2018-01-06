#ifndef VDBG_SERVER_H_
#define VDBG_SERVER_H_

#include <imdbg.h>
#include <vdbg.h>

#include <vector>
#include <thread>
#include <mutex>

namespace vdbg
{
class Server
{
  public:
   bool start( const char* name, int connections );
   void stop();

   void getPendingProfilingTraces(
       std::vector<std::vector<DisplayableTrace> >& tracesFrame,
       std::vector<std::vector<char> >& stringData,
       std::vector<uint32_t>& threadIds );

  private:
   bool handleNewMessage( int clientId, uint32_t threadId );

   std::thread _thread;
   bool _running{false};
   details::SharedMemory _sharedMemory;

   std::mutex pendingTracesMutex;
   std::vector< std::vector< DisplayableTrace > > pendingTraces;
   std::vector< std::vector< char > > pendingStringData;
   std::vector< uint32_t > pendingThreadIds;
};

}  // namespace vdbg

#endif // VDBG_SERVER_H_