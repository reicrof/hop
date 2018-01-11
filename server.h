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
   void setRecording( bool recording );
   void stop();

   void getPendingProfilingTraces(
       std::vector<std::vector<DisplayableTrace> >& tracesFrame,
       std::vector<std::vector<char> >& stringData,
       std::vector<uint32_t>& threadIds );

  private:
   // Returns the number of bytes processed
   size_t handleNewMessage( uint8_t* data, size_t maxSize );

   std::thread _thread;
   bool _running{false};
   SharedMemory _sharedMem;

   std::mutex pendingTracesMutex;
   std::vector< std::vector< DisplayableTrace > > pendingTraces;
   std::vector< std::vector< char > > pendingStringData;
   std::vector< uint32_t > pendingThreadIds;
};

}  // namespace vdbg

#endif // VDBG_SERVER_H_