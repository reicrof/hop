#ifndef SNOOPER_H_
#define SNOOPER_H_

#define HOP_VIEWER
#include "Hop.h"

#include <vector>

namespace hop
{
class Viewer;
class ProfilerView;
class Snooper
{
   std::vector<std::unique_ptr<NetworkConnection>> _pending_connections;
   std::vector<std::unique_ptr<ProfilerView>> _profilers;
   char _addressStr[sizeof( NetworkConnection::_addressStr )];
   char _portStr[sizeof( NetworkConnection::_portStr )];
   int32_t _selectedConnection;
   bool _windowOpen;

  public:
   Snooper();
   void enable();
   void draw( Viewer* viewer );
};

}  // namespace hop

#endif  // SNOOPER_H_