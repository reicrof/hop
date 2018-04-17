#include "Options.h"

#include <fstream>

static const char* startFullScreenStr = "start_full_screen";

namespace hop
{

Options g_options = {};

bool saveOptions()
{
   std::ofstream outOptions( "hop.conf" );
   if( outOptions.is_open() )
   {
      outOptions << startFullScreenStr << " " << (g_options.startFullScreen ? 1 : 0 << '\n');
      return true;
   }

   return false;
}

bool loadOptions()
{
   std::ifstream inOptions( "hop.conf" );
   std::string token;
   if( inOptions.is_open() )
   {
      inOptions >> token;
      if( token == startFullScreenStr )
      {
         inOptions >> g_options.startFullScreen;
      }

      return true;
   }
   return false;
}

} // namespace hop