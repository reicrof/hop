#include "ProfilerView.h"
#include "TimelineTracksView.h"
#include "Hop.h"
#include "common/TimelineTrack.h"
#include "common/Utils.h"
#include "common/TraceData.h"
#include "imgui/imgui.h"

#include "Lod.h"
#include "ModalWindow.h"
#include "Options.h"
#include "RendererGL.h"
#include <SDL_keycode.h>

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>

extern bool g_run;

static void displayBackgroundHelpMsg( uint32_t windowWidth, uint32_t windowHeight )
{
   const char* helpTxt =
       "-------------- Hop --------------\n\n"
       "Press 'R' to start/stop recording\n"
       "Right mouse click to get traces details\n"
       "Double click on a trace to focus it\n"
       "Right mouse drag to zoom on a region\n"
       "Left mouse drag to measure time in region\n"
       "Right click on the timeline to create a bookmark\n"
       "Use arrow keys <-/-> to navigate bookmarks\n"
       "Use CTRL+F to search traces\n"
       "Use Del to delete traces\n";
   const auto pos = ImGui::GetWindowPos();
   ImDrawList* DrawList = ImGui::GetWindowDrawList();
   auto size = ImGui::CalcTextSize( helpTxt );
   DrawList->AddText(
       ImGui::GetIO().Fonts->Fonts[0],
       30.0f,
       ImVec2( pos.x + windowWidth / 2 - ( size.x ), pos.y + windowHeight / 2 - size.y ),
       ImGui::GetColorU32( ImGuiCol_TextDisabled ),
       helpTxt );
}

static int closestLodLevel( hop::TimeDuration timelineDuration )
{
   int lodLvl = 0;
   while( lodLvl < hop::LOD_COUNT - 1 && timelineDuration > hop::LOD_CYCLES[lodLvl] )
   {
      ++lodLvl;
   }
   return lodLvl;
}

hop::ProfilerView::ProfilerView( hop::Profiler::SourceType type, int processId, const char* str )
   : _profiler( type, processId, str ), _lodLevel( 0 ), _highlightValue( 0.0f )
{
}

void hop::ProfilerView::fetchClientData()
{
   _profiler.fetchClientData();
}

void hop::ProfilerView::update( float globalTimeMs, TimeDuration timelineDuration )
{
   HOP_PROF_FUNC();
   _highlightValue = (std::sin( 0.007f * globalTimeMs ) * 0.8f + 1.0f) / 2.0f;

   // Update current lod level
   _lodLevel = closestLodLevel( timelineDuration );

   _trackViews.update( _profiler );
}

void hop::ProfilerView::setRecording( bool recording )
{
   _profiler.setRecording( recording );
}

bool hop::ProfilerView::saveToFile( const char* path )
{
   return _profiler.saveToFile( path );
}

bool hop::ProfilerView::openFile( const char* path )
{
   return _profiler.openFile( path );
}

void hop::ProfilerView::draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   if ( _trackViews.count() == 0 && !data().recording() )
   {
      displayBackgroundHelpMsg( ImGui::GetWindowWidth(), ImGui::GetWindowHeight() );
   }
   else
   {
      TimelineTrackDrawData drawData = { _profiler, tlInfo, _lodLevel, _highlightValue };
      _trackViews.draw( drawData, msgArray );
   }
}

bool hop::ProfilerView::handleHotkey()
{
   bool handled = false;
   // Let the tracks handle the hotkeys first.
   if( _trackViews.handleHotkeys() )
   {
      return true;
   }

   // Otherwise, let the profiler handle it
   if ( ImGui::IsKeyDown( ImGui::GetKeyIndex( ImGuiKey_Delete ) ) && _trackViews.count() > 0 )
   {
      if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) &&
           !hop::modalWindowShowing() )
      {
         hop::displayModalWindow(
             "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
         handled = true;
      }
   }

   return handled;
}

bool hop::ProfilerView::handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel )
{
   return _trackViews.handleMouse( posX, posY, lmClicked, rmClicked, wheel );
}

void hop::ProfilerView::clear()
{
   _profiler.clear();
   _trackViews.clear();
}

float hop::ProfilerView::canvasHeight() const
{
   return 9999.0f;
}

int hop::ProfilerView::lodLevel() const
{
   return _lodLevel;
}

const hop::Profiler& hop::ProfilerView::data() const
{
   return _profiler;
}
