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

namespace hop
{

ProfilerView::ProfilerView( Profiler::SourceType type, int processId, const char* str )
   : _profiler( type, processId, str ), _type( Type::PROFILER ), _lodLevel( 0 ), _highlightValue( 0.0f )
{
   const int viewTypesCount = (int)Type::COUNT;
   for( int i = 0; i < viewTypesCount; ++i )
      _viewsVerticalPos[i] = 0.0f;

   // Init scroll for the stat view
   const float windowHeight = ImGui::GetIO().DisplaySize.y;
   _viewsVerticalPos[(int)Type::STATS] = TimelineStatsView::originScrollAmount( windowHeight );
}

void ProfilerView::fetchClientData()
{
   _profiler.fetchClientData();
}

void ProfilerView::update( float globalTimeMs, TimeDuration timelineDuration )
{
   HOP_PROF_FUNC();
   _highlightValue = (std::sin( 0.007f * globalTimeMs ) * 0.8f + 1.0f) / 2.0f;

   // Update current lod level
   _lodLevel = closestLodLevel( timelineDuration );

   _trackViews.update( _profiler );
}

void ProfilerView::setRecording( bool recording )
{
   _profiler.setRecording( recording );
}

bool ProfilerView::saveToFile( const char* path )
{
   return _profiler.saveToFile( path );
}

bool ProfilerView::openFile( const char* path )
{
   return _profiler.openFile( path );
}

ProfilerView::Type ProfilerView::type() const
{
   return _type;
}

void ProfilerView::setType( Type type )
{
   _type = type;
}

float ProfilerView::verticalPos() const
{
   return _viewsVerticalPos[(int)_type];
}

void ProfilerView::setVerticalPos( float pos )
{
   _viewsVerticalPos[(int)_type] = pos;
}

void ProfilerView::draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   if ( _type == Type::PROFILER && _trackViews.count() > 0 )
   {
      TimelineTrackDrawData drawData = { _profiler, tlInfo, _lodLevel, _highlightValue };
      _trackViews.draw( drawData, msgArray );
   }
   else if( _type == Type::STATS )
   {
      TimelineStatsDrawData drawData = { _profiler, tlInfo, _lodLevel, _highlightValue };
      _timelineStats.draw( drawData, msgArray );
   }

   static int64_t frameRendered = 0;
   HOP_STATS_INT64( "Frame Rendered", ++frameRendered );
}

bool ProfilerView::handleHotkey()
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

bool ProfilerView::handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel )
{
   return _trackViews.handleMouse( posX, posY, lmClicked, rmClicked, wheel );
}

void ProfilerView::clear()
{
   _profiler.clear();
   _trackViews.clear();
}

float ProfilerView::canvasHeight() const
{
   return 9999.0f;
}

int ProfilerView::lodLevel() const
{
   return _lodLevel;
}

const Profiler& ProfilerView::data() const
{
   return _profiler;
}

} // namespace hop