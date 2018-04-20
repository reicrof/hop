#include "Options.h"

#include <cstring>
#include <fstream>

#include "imgui/imgui.h"

static const char* startFullScreenStr = "start_full_screen";
static const char* traceHeights = "trace_height";
static const char* threadColors = "thread_colors";
static const char* glFinishByDefault = "gl_finish_by_default";

static const uint32_t DEFAULT_COLORS[] = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8, 0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff, 0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080, 0xffFFFFFF, 0xff000000};

static constexpr uint32_t DEFAULT_COLORS_SIZE = sizeof(DEFAULT_COLORS) / sizeof( DEFAULT_COLORS[0] );

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

      // Use gl finish by defualt option
      outOptions << glFinishByDefault << " " << (g_options.glFinishByDefault ? 1 : 0) << '\n';

      // Trace height option
      outOptions << traceHeights << " " << g_options.traceHeight << '\n';

      // Thread colors options
      outOptions << threadColors << " " << g_options.threadColors.size() << " " ;
      for( auto c : g_options.threadColors )
      {
         outOptions << c << " ";
      }
      return true;
   }

   return false;
}

bool loadOptions()
{
   // Even without a config file we load the default thread colors first
   g_options.threadColors.assign( DEFAULT_COLORS, DEFAULT_COLORS+DEFAULT_COLORS_SIZE );

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
         else if( strcmp( token.c_str(), glFinishByDefault ) == 0 )
         {
            inOptions >> g_options.glFinishByDefault;
         }
         else if( strcmp( token.c_str(), traceHeights ) == 0 )
         {
            inOptions >> g_options.traceHeight;
         }
         else if( strcmp( token.c_str(), threadColors ) == 0 )
         {
            size_t colorCount = 0;
            inOptions >> colorCount;
            g_options.threadColors.resize( colorCount, 0 );
            for( size_t i = 0; i < colorCount; ++i )
            {
               inOptions >> g_options.threadColors[i];
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
   if ( ImGui::Begin( "Options", &opt.optionWindowOpened ) )
   {
      ImGui::Checkbox( "Start in Fullscreen", &opt.startFullScreen );
      ImGui::Checkbox( "glFinish() by default", &opt.glFinishByDefault );
      ImGui::SliderFloat( "Trace Height", &opt.traceHeight, 15.0f, 50.0f );

      ImGui::Spacing();

      if ( ImGui::CollapsingHeader( "Thread Colors" ) )
      {
         ImColor color;
         for( size_t i = 0; i < opt.threadColors.size(); ++i )
         {
            color = opt.threadColors[i];
            ImGui::PushID( i );
            ImGui::Text( "Thread #%d", (int)i );
            ImGui::SameLine();
            ImGui::ColorEdit3( "Color test", (float*)&color.Value );
            ImGui::PopID();
            opt.threadColors[i] = color;
         }
      }
   }
   ImGui::End();
   ImGui::PopStyleColor();
}

uint32_t getColorForThread( const Options& opt, uint32_t threadIdx )
{
   return opt.threadColors[ threadIdx % opt.threadColors.size() ];
}

void setThreadCount( Options& opt, uint32_t threadCount )
{
   const uint32_t curThreadCount = opt.threadColors.size();
   if( threadCount > curThreadCount )
   {
      opt.threadColors.resize( threadCount );
      for( uint32_t i = curThreadCount; i < threadCount; ++i )
      {
         opt.threadColors[ i ] = opt.threadColors[ i % DEFAULT_COLORS_SIZE ];
      }
   }
}

} // namespace hop