#include "hop/ProfilerView.h"
#include "hop/TimelineTracksView.h"

#include "hop/Lod.h"
#include "hop/ModalWindow.h"
#include "hop/Options.h"
#include "hop/RendererGL.h"

#include "Hop.h"

#include "common/TimelineTrack.h"
#include "common/Utils.h"
#include "common/TraceData.h"

#include "imgui/imgui.h"
#include <SDL_keycode.h>

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>

extern bool g_run;

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

hop::ProfilerView::ProfilerView( NetworkConnection& nc )
    : _profiler( nc ), _lodLevel( 0 ), _highlightValue( 0.0f )
{
}

bool hop::ProfilerView::fetchClientData()
{
   return _profiler.fetchClientData();
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

bool hop::ProfilerView::draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   bool needs_redraw = false;
   if ( _trackViews.count() > 0 )
   {
      TimelineTrackDrawData drawData = { _profiler, tlInfo, _lodLevel, _highlightValue };
      needs_redraw = _trackViews.draw( drawData, msgArray );
   }
   return needs_redraw;
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
             "Delete all traces?", nullptr, hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
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
