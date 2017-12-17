#ifndef VDBG_SERVER_H_
#define VDBG_SERVER_H_

#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <thread>

namespace vdbg
{
enum class MsgType:uint32_t;
class Server
{
  public:
   bool start( const char* name, int connections );
   void stop();

  private:
   bool handleNewConnection();
   bool handleNewMessage( int clientId, vdbg::MsgType type, uint32_t size );

   std::thread _thread;
   std::string _serverName;
   std::vector<int> _clients;
   fd_set _fdSet{};
   int _socket{-1};
   bool _running{false};
};

}  // namespace vdbg

#endif // VDBG_SERVER_H_