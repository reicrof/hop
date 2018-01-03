#ifndef VDBG_SERVER_H_
#define VDBG_SERVER_H_

#include <imdbg.h>
#include <message.h>

#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

namespace vdbg
{
enum class MsgType:uint32_t;
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
   bool handleNewConnection();
   bool handleNewMessage( int clientId, MsgType type, uint32_t size );

   std::thread _thread;
   std::string _serverName;
   std::vector<int> _clients;
   fd_set _fdSet{};
   int _socket{-1};
   bool _running{false};

   std::mutex pendingTracesMutex;
   std::vector< std::vector< DisplayableTrace > > pendingTraces;
   std::vector< std::vector< char > > pendingStringData;
   std::vector< uint32_t > pendingThreadIds;
};

}  // namespace vdbg

#endif // VDBG_SERVER_H_