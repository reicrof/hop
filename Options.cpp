#include "Options.h"

#include <cstring>
#include <fstream>

#include "imgui/imgui.h"

static const char* startFullScreenStr = "start_full_screen";
static const char* traceHeights = "trace_height";

namespace hop
{

Options g_options = {};

bool saveOptions()
{
   std::ofstream outOptions( "hop.conf" );
   if( outOptions.is_open() )
   {
      outOptions << startFullScreenStr << " " << (g_options.startFullScreen ? 1 : 0) << '\n';
      outOptions << traceHeights << " " << g_options.traceHeight << '\n';
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
      while( inOptions >> token )
      {
         if( strcmp( token.c_str(), startFullScreenStr ) == 0 )
         {
            inOptions >> g_options.startFullScreen;
         }
         else if( strcmp( token.c_str(), traceHeights ) == 0 )
         {
            inOptions >> g_options.traceHeight;
         }
      }

      return true;
   }
   return false;
}

void drawOptionsWindow( Options& opt )
{
   if( !opt.optionWindowOpened )
      return;

   static float col[4] = {};
   ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.20f, 0.20f, 0.75f));
   if ( ImGui::Begin( "Options", &opt.optionWindowOpened ) )
   {
      ImGui::Checkbox( "Start in Fullscreen", &opt.startFullScreen );
      ImGui::SliderFloat( "Trace Height", &opt.traceHeight, 5.0f, 50.0f );
      ImGui::ColorEdit4( "Color test", col );
   }
   ImGui::End();
   ImGui::PopStyleColor();
}

} // namespace hop