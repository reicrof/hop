#ifndef VDBG_CLIENT_H_
#define VDBG_CLIENT_H_

#include <stdint.h>

namespace vdbg
{
class Client
{
  public:
   bool connect( const char* serverName );
   bool send( uint8_t* data, uint32_t size ) const;
   void disconnect();

  private:
   int _socket{-1};
};
} // namespace vdbg

#endif // VDBG_CLIENT_H_