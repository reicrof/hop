#include "Options.h"

#include <cstring>
#include <string>
#include <fstream>

#include "imgui/imgui.h"

static const char* startFullScreenToken = "start_full_screen";
static const char* traceHeightsToken = "trace_height";
static const char* windowOpacityToken = "window_opacity";
static const char* zoneColorsToken = "zone_colors";
static const char* debugWindowToken = "show_debug_window";
static const char* showCoreInfoToken = "show_core_info";
static const char* vsyncOnToken = "vsync_on";

static const uint32_t DEFAULT_COLORS[] = {
    0xff375fbc, 0xff3cb44b, 0xffffe119, 0xff0082c8, 0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff, 0xffaa6e28, 0xfffffac8, 0xff800000, 0xffe6194b,
    0xff0000ff };

namespace hop
{
struct Options
{
   float traceHeight{20.0f};
   float windowOpacity{0.75f};
   bool startFullScreen{true};
   bool vsyncOn{true};
   bool showDebugWindow{false};
   bool showCoreInfo{true};
   std::array< uint32_t, HOP_MAX_ZONE_COLORS + 1 > zoneColors;
   bool optionWindowOpened{false};
} g_options = {};

float options::traceHeight()
{
   return g_options.traceHeight;
}

float options::windowOpacity()
{
   return g_options.windowOpacity;
}

bool options::fullscreen()
{
   return g_options.startFullScreen;
}

bool options::vsyncOn()
{
   return g_options.vsyncOn;
}

bool options::showDebugWindow()
{
   return g_options.showDebugWindow;
}

bool options::showCoreInfo()
{
   return g_options.showCoreInfo;
}

const std::array< uint32_t, HOP_MAX_ZONE_COLORS + 1 >& options::zoneColors()
{
   return g_options.zoneColors;
}

bool options::save()
{
   std::ofstream outOptions( "hop.conf" );
   if( outOptions.is_open() )
   {
      // Full screen option
      outOptions << startFullScreenToken << " " << (g_options.startFullScreen ? 1 : 0) << '\n';

      // Display debug window
      outOptions << debugWindowToken << " " << (g_options.showDebugWindow ? 1 : 0) << '\n';

      // Display core information
      outOptions << showCoreInfoToken << " " << (g_options.showCoreInfo ? 1 : 0) << '\n';

      // Vsync state
      outOptions << vsyncOnToken << " " << (g_options.vsyncOn ? 1 : 0) << '\n';

      // Trace height option
      outOptions << traceHeightsToken << " " << g_options.traceHeight << '\n';

      // Opacity of the window
      outOptions << windowOpacityToken << " " << g_options.windowOpacity << '\n';

      // Zone colors options
      outOptions << zoneColorsToken << " " << g_options.zoneColors.size() << " " ;
      for( auto c : g_options.zoneColors )
      {
         outOptions << c << " ";
      }
      return true;
   }

   return false;
}

bool options::load()
{
   // Even without a config file we load the default zone colors first
   for( size_t i = 0; i < g_options.zoneColors.size(); ++i )
   {
      g_options.zoneColors[i] = DEFAULT_COLORS[i];
   }

   std::ifstream inOptions( "hop.conf" );
   std::string token;
   if( inOptions.is_open() )
   {
      while( inOptions >> token )
      {
         if( strcmp( token.c_str(), startFullScreenToken ) == 0 )
         {
            inOptions >> g_options.startFullScreen;
         }
         else if( strcmp( token.c_str(), vsyncOnToken ) == 0 )
         {
            inOptions >> g_options.vsyncOn;
         }
         else if( strcmp( token.c_str(), debugWindowToken ) == 0 )
         {
            inOptions >> g_options.showDebugWindow;
         }
         else if( strcmp( token.c_str(), showCoreInfoToken ) == 0 )
         {
            inOptions >> g_options.showCoreInfo;
         }
         else if( strcmp( token.c_str(), traceHeightsToken ) == 0 )
         {
            inOptions >> g_options.traceHeight;
         }
         else if( strcmp( token.c_str(), windowOpacityToken ) == 0 )
         {
            inOptions >> g_options.windowOpacity;
         }
         else if( strcmp( token.c_str(), zoneColorsToken ) == 0 )
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

void options::enableOptionWindow()
{
   g_options.optionWindowOpened = true;
}

void options::draw()
{
   if ( !g_options.optionWindowOpened ) return;

   ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, g_options.windowOpacity ) );
   if ( ImGui::Begin( "Options", &g_options.optionWindowOpened, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      ImGui::Checkbox( "Start in Fullscreen", &g_options.startFullScreen );
      ImGui::Checkbox("Show Debug Window", &g_options.showDebugWindow );
      ImGui::Checkbox("Show Core Information", &g_options.showCoreInfo );
      ImGui::Checkbox("Vsync Enabled", &g_options.vsyncOn );
      ImGui::SliderFloat( "Trace Height", &g_options.traceHeight, 15.0f, 50.0f );
      ImGui::SliderFloat( "Window Opacity", &g_options.windowOpacity, 0.0f, 1.0f );

      ImGui::Spacing();

      if ( ImGui::CollapsingHeader( "Zone Colors" ) )
      {
         size_t i = HOP_MAX_ZONE_COLORS - 1;
         ImColor color = g_options.zoneColors[i];
         ImGui::PushID( i );
         ImGui::Text( "General Zone" );
         ImGui::SameLine();
         ImGui::ColorEdit3( "", (float*)&color.Value );
         ImGui::PopID();
         g_options.zoneColors[i] = color;

         for( i = 0; i < g_options.zoneColors.size() - 2; ++i )
         {
            color = g_options.zoneColors[i];
            ImGui::PushID( i );
            ImGui::Text( "Zone #%d", (int)i+1 );
            ImGui::SameLine();
            ImGui::ColorEdit3( "", (float*)&color.Value );
            ImGui::PopID();
            g_options.zoneColors[i] = color;
         }

         i = HOP_MAX_ZONE_COLORS; // Locks uses index 16 for color infos
         color = g_options.zoneColors[i];
         ImGui::PushID( i );
         ImGui::Text( "Locks" );
         ImGui::SameLine();
         ImGui::ColorEdit3( "", (float*)&color.Value );
         ImGui::PopID();
         g_options.zoneColors[i] = color;
      }
   }
   ImGui::End();
   ImGui::PopStyleColor();
}

} // namespace hop