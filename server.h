#ifndef VDBG_SERVER_H_
#define VDBG_SERVER_H_

#include <message.h>
#include <imdbg.h>

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

   void getProfilingTraces(std::vector< vdbg::DisplayableTraceFrame >& tracesFrame );

  private:
   bool handleNewConnection();
   bool handleNewMessage( int clientId, vdbg::MsgType type, uint32_t size );

   std::thread _thread;
   std::string _serverName;
   std::vector<int> _clients;
   fd_set _fdSet{};
   int _socket{-1};
   bool _running{false};

   std::mutex pendingTracesMutex;
   std::vector< vdbg::DisplayableTraceFrame > pendingTraces;
};

}  // namespace vdbg

#endif // VDBG_SERVER_H_