#include "Options.h"

#include <cstring>
#include <string>
#include <fstream>

#include "imgui/imgui.h"

static const char* startFullScreenStr = "start_full_screen";
static const char* traceHeights = "trace_height";
static const char* zoneColors = "zone_colors";
static const char* debugWindow = "show_debug_window";
static const char* vsyncOn = "vsync_on";

static const uint32_t DEFAULT_COLORS[] = {
    0xff5a00be, 0xff3cb44b, 0xffffe119, 0xff0082c8, 0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff, 0xffaa6e28, 0xfffffac8, 0xff800000, 0xffe6194b,
    0xff0000ff };

namespace hop
{

Options g_options = {};

bool saveOptions()
{
   std::ofstream outOptions( "hop.conf" );
   if( outOptions.is_open() )
   {
      // Full screen option
      outOptions << startFullScreenStr << " " << (g_options.startFullScreen ? 1 : 0) << '\n';

      // Display debug window
      outOptions << debugWindow << " " << (g_options.debugWindow ? 1 : 0) << '\n';

      // Vsync state
      outOptions << vsyncOn << " " << (g_options.vsyncOn ? 1 : 0) << '\n';

      // Trace height option
      outOptions << traceHeights << " " << g_options.traceHeight << '\n';

      // Zone colors options
      outOptions << zoneColors << " " << g_options.zoneColors.size() << " " ;
      for( auto c : g_options.zoneColors )
      {
         outOptions << c << " ";
      }
      return true;
   }

   return false;
}

bool loadOptions()
{
   // Even without a config file we load the default zone colors first
   for( size_t i = 0; i < g_options.zoneColors.size(); ++i )
   {
      g_options.zoneColors[i] = DEFAULT_COLORS[i];
      g_options.zoneEnabled[i] = true;
   }

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
         else if( strcmp( token.c_str(), vsyncOn ) == 0 )
         {
            inOptions >> g_options.vsyncOn;
         }
         else if( strcmp( token.c_str(), debugWindow ) == 0 )
         {
            inOptions >> g_options.debugWindow;
         }
         else if( strcmp( token.c_str(), traceHeights ) == 0 )
         {
            inOptions >> g_options.traceHeight;
         }
         else if( strcmp( token.c_str(), zoneColors ) == 0 )
         {
            size_t colorCount = 0;
            inOptions >> colorCount;
            const size_t minColCount = std::min( colorCount, g_options.zoneColors.size() );
            for( size_t i = 0; i < minColCount; ++i )
            {
               inOptions >> g_options.zoneColors[i];
               g_options.zoneColors[i];
            }
         }
      }

      return true;
   }
   return false;
}

void drawOptionsWindow( Options& opt )
{
   if ( !opt.optionWindowOpened ) return;

   ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, 0.75f ) );
   if ( ImGui::Begin( "Options", &opt.optionWindowOpened, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      ImGui::Checkbox( "Start in Fullscreen", &opt.startFullScreen );
      ImGui::Checkbox("Show Debug Window", &opt.debugWindow );
      ImGui::Checkbox("Vsync Enabled", &opt.vsyncOn );
      ImGui::SliderFloat( "Trace Height", &opt.traceHeight, 15.0f, 50.0f );

      ImGui::Spacing();

      if ( ImGui::CollapsingHeader( "Zone Colors" ) )
      {
         ImGui::SliderFloat( "Disabled Zones Opacity", &opt.disabledZoneOpacity, 0.0f, 1.0f, "%.2f" );

         size_t i = HOP_MAX_ZONE_COLORS - 1;
         ImColor color = opt.zoneColors[i];
         ImGui::PushID( i );
         ImGui::Checkbox( "", &opt.zoneEnabled[i] );
         ImGui::SameLine();
         ImGui::Text( "General Zone" );
         ImGui::SameLine();
         ImGui::ColorEdit3( "", (float*)&color.Value );
         ImGui::PopID();
         opt.zoneColors[i] = color;

         for( i = 0; i < opt.zoneColors.size() - 2; ++i )
         {
            color = opt.zoneColors[i];
            ImGui::PushID( i );
            ImGui::Checkbox( "", &opt.zoneEnabled[i] );
            ImGui::SameLine();
            ImGui::Text( "Zone #%d", (int)i+1 );
            ImGui::SameLine();
            ImGui::ColorEdit3( "", (float*)&color.Value );
            ImGui::PopID();
            opt.zoneColors[i] = color;
         }

         i = HOP_MAX_ZONE_COLORS; // Locks uses index 16 for color infos
         color = opt.zoneColors[i];
         ImGui::PushID( i );
         ImGui::Checkbox( "", &opt.zoneEnabled[i] );
         ImGui::SameLine();
         ImGui::Text( "Locks" );
         ImGui::SameLine();
         ImGui::ColorEdit3( "", (float*)&color.Value );
         ImGui::PopID();
         opt.zoneColors[i] = color;
      }
   }
   ImGui::End();
   ImGui::PopStyleColor();
}

} // namespace hop