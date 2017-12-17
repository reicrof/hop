#include <server.h>
#include <message.h>

int main()
{
   vdbg::Server serv;
   serv.start( vdbg::SERVER_PATH, 10 );
   using namespace std::chrono_literals;
   std::this_thread::sleep_for( 10s );
   serv.stop();
}
