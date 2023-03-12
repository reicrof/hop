#include "hop/Options.h"
#include "common/Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <fstream>

static const char* startFullScreenToken = "start_full_screen";
static const char* displayScalingToekn  = "display_scaling";
static const char* traceHeightsToken    = "trace_height";
static const char* traceTextAlignToken  = "trace_text_alignment";
static const char* windowOpacityToken   = "window_opacity";
static const char* zoneColorsToken      = "zone_colors";
static const char* debugWindowToken     = "show_debug_window";
static const char* showCoreInfoToken    = "show_core_info";
static const char* vsyncOnToken         = "vsync_on";

// Persisted data
static const char* lastAddressUsedToken = "last_address_used";

static const uint32_t DEFAULT_COLORS[] = {
    0xffe7073e, 0xffd64c91, 0xffa9842d, 0xff0082c8, 0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xff8da524, 0xfffabebe, 0xff008080, 0xffe6beff, 0xffaa6e28, 0xfffffac8, 0xff800000, 0xff375fbc,
    0xff0000ff };

static constexpr int MAX_ADDR_LEN = 64;

namespace hop
{
struct Options
{
   float displayScaling{1.0f};
   float traceHeight{20.0f};
   float traceTextAlignment{0.5f};
   float windowOpacity{0.8f};
   bool startFullScreen{true};
   bool vsyncOn{true};
   bool showDebugWindow{false};
   bool showCoreInfo{true};
   bool optionWindowOpened{false};
   char lastAddressUsed[MAX_ADDR_LEN] = "localhost";
   std::array< uint32_t, HOP_ZONE_MAX + 1 > zoneColors;
} g_options = {};

float options::displayScaling()
{
   return g_options.displayScaling;
}

float options::traceHeight()
{
   return g_options.traceHeight;
}

float options::traceTextAlignment()
{
   return g_options.traceTextAlignment;
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

const char* options::lastAddressUsed()
{
   return g_options.lastAddressUsed;
}

void options::setLastAddressUsed (const char* addr)
{
   if (!addr) return;

   size_t len = strlen (addr);
   if( len < MAX_ADDR_LEN )
      strncpy( g_options.lastAddressUsed, addr, MAX_ADDR_LEN );
}

const std::array< uint32_t, HOP_ZONE_MAX + 1 >& options::zoneColors()
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

      // Display scaling
      outOptions << displayScalingToekn << " " << g_options.displayScaling << '\n';

      // Trace height option
      outOptions << traceHeightsToken << " " << g_options.traceHeight << '\n';

      // Trace text alignement inside the rectangle
      outOptions << traceTextAlignToken << " " << g_options.traceTextAlignment << '\n';

      // Opacity of the window
      outOptions << windowOpacityToken << " " << g_options.windowOpacity << '\n';

      // Last address used
      outOptions << lastAddressUsedToken << " " << g_options.lastAddressUsed << '\n';

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
   const uint32_t defaultColorCount = sizeof( DEFAULT_COLORS ) / sizeof( DEFAULT_COLORS[0] );
   memcpy( g_options.zoneColors.data(), DEFAULT_COLORS, defaultColorCount * sizeof( uint32_t ) );
   // Once the default colors are copied, fill the rest of the array with the default color
   std::fill(
       g_options.zoneColors.begin() + defaultColorCount, g_options.zoneColors.end(),
       DEFAULT_COLORS[0] );

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
         else if( strcmp( token.c_str(), displayScalingToekn ) == 0 )
         {
            inOptions >> g_options.displayScaling;
         }
         else if( strcmp( token.c_str(), traceHeightsToken ) == 0 )
         {
            inOptions >> g_options.traceHeight;
         }
         else if( strcmp( token.c_str(), traceTextAlignToken ) == 0 )
         {
            inOptions >> g_options.traceTextAlignment;
         }
         else if( strcmp( token.c_str(), windowOpacityToken ) == 0 )
         {
            inOptions >> g_options.windowOpacity;
         }
         else if (strcmp( token.c_str(), lastAddressUsedToken ) == 0 )
         {
            inOptions >> g_options.lastAddressUsed;
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

void options::setDisplayScaling(float scaling)
{
   g_options.displayScaling = hop::clamp( scaling, 0.5f, 5.0f );
}

void options::draw()
{
   if ( !g_options.optionWindowOpened ) return;

   ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, g_options.windowOpacity ) );
   if ( ImGui::Begin( "Options", &g_options.optionWindowOpened, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      static bool options_dirty = false;
      options_dirty |= ImGui::Checkbox( "Start in Fullscreen", &g_options.startFullScreen );
      options_dirty |= ImGui::Checkbox("Show Debug Window", &g_options.showDebugWindow );
      options_dirty |= ImGui::Checkbox("Show Core Information", &g_options.showCoreInfo );
      options_dirty |= ImGui::Checkbox("Vsync Enabled", &g_options.vsyncOn );
      options_dirty |= ImGui::InputFloat( "Display Scaling", &g_options.displayScaling, 0.25f, 0.25f, "%.2f" );
      options_dirty |= ImGui::SliderFloat( "Trace Height", &g_options.traceHeight, 15.0f, 50.0f );
      options_dirty |= ImGui::SliderFloat( "Trace Text Alignment", &g_options.traceTextAlignment, 0.0f, 1.0f );
      options_dirty |= ImGui::SliderFloat( "Window Opacity", &g_options.windowOpacity, 0.0f, 1.0f );

      // Make sure to clamp the valid to something reasonable
      setDisplayScaling( g_options.displayScaling );

      ImGui::Spacing();

      if ( ImGui::CollapsingHeader( "Zone Colors" ) )
      {
         size_t i = 0;
         ImColor color = g_options.zoneColors[i];
         ImGui::PushID( i );
         ImGui::Text( "Default Zone" );
         ImGui::SameLine();
         options_dirty |=ImGui::ColorEdit3( "", (float*)&color.Value );
         ImGui::PopID();
         g_options.zoneColors[i] = color;

         for( i = 1; i < g_options.zoneColors.size(); ++i )
         {
            color = g_options.zoneColors[i];
            ImGui::PushID( i );
            ImGui::Text( "Zone #%d", (int)i );
            ImGui::SameLine();
            options_dirty |=ImGui::ColorEdit3( "", (float*)&color.Value );
            ImGui::PopID();
            g_options.zoneColors[i] = color;
         }
      }

      if( options_dirty && ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
      {
         save();
         options_dirty = false;
      }
   }
   ImGui::End();
   ImGui::PopStyleColor();
}

} // namespace hop
