#include "Timeline.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "Lod.h"
#include "ModalWindow.h"
#include "Stats.h"
#include "StringDb.h"
#include "TraceDetail.h"
#include "Options.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <SDL_keycode.h>

#include <thread>
#include <cmath>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static constexpr hop::TimeDuration MIN_NANOS_TO_DISPLAY = 500;
static constexpr hop::TimeDuration MAX_NANOS_TO_DISPLAY = 900000000000;
static constexpr float MIN_TRACE_LENGTH_PXL = 1.0f;
static constexpr float MAX_TRACE_HEIGHT = 50.0f;
static constexpr float MIN_TRACE_HEIGHT = 15.0f;
static constexpr uint32_t DISABLED_COLOR = 0xFF505050;
static constexpr uint32_t HOVERED_COLOR_DELTA = 0x00191919;
static constexpr uint32_t ACTIVE_COLOR_DELTA = 0x00333333;
static constexpr float PADDING_BETWEEN_THREADS = 70.0f;

static void drawHoveringTimelineLine(float posInScreenX, float timelineStartPosY, const char* text )
{
   HOP_PROF_FUNC();

   constexpr float LINE_PADDING = 5.0f;
   constexpr float TEXT_PADDING = 10.0f;
   
   auto drawList = ImGui::GetWindowDrawList();
   drawList->PushClipRectFullScreen();
   drawList->AddLine(
      ImVec2(posInScreenX, timelineStartPosY + LINE_PADDING),
      ImVec2(posInScreenX, 9999),
      ImColor(255, 255, 255, 200),
      1.5f);
   drawList->AddText( ImVec2( posInScreenX, timelineStartPosY - TEXT_PADDING), ImColor(255,255,255), text);
   drawList->PopClipRect();
}

static void drawBookmarks( float posXPxl, float posYPxl )
{
   HOP_PROF_FUNC();

   constexpr float BOOKMARK_WIDTH = 8.0f;
   constexpr float BOOKMARK_HEIGHT = 20.0f;
   constexpr float LINE_PADDING = 5.0f;
   ImGui::SetCursorScreenPos( ImVec2( posXPxl - (BOOKMARK_WIDTH * 0.5f), posYPxl + LINE_PADDING ) );
   ImGui::Button("", ImVec2( BOOKMARK_WIDTH, BOOKMARK_HEIGHT ) );
}

namespace hop
{

float Timeline::TRACE_HEIGHT = 20.0f;
float Timeline::TRACE_VERTICAL_PADDING = 2.0f;
float Timeline::PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

void Timeline::update( float deltaTimeMs ) noexcept
{
   HOP_PROF_FUNC();

   switch ( _animationState.type )
   {
      case ANIMATION_TYPE_NONE:
         _timelineStart = _animationState.targetTimelineStart;
         _timelineRange = _animationState.targetTimelineRange;
         break;
      case ANIMATION_TYPE_NORMAL:
      case ANIMATION_TYPE_FAST:
      {
         int64_t speedFactor = _animationState.type == ANIMATION_TYPE_NORMAL ? 100 / deltaTimeMs : 90 / deltaTimeMs;
         speedFactor = std::max( speedFactor, (int64_t)1 );
         if ( std::abs( _timelineStart - _animationState.targetTimelineStart ) > 0.00001 )
         {
            int64_t delta = _animationState.targetTimelineStart - _timelineStart;
            if ( std::abs( delta ) < 10 )
            {
               _timelineStart = _animationState.targetTimelineStart;
            }
            else
            {
               _timelineStart += delta / speedFactor;
            }
         }

         if ( std::abs( _timelineRange - _animationState.targetTimelineRange ) > 0.00001 )
         {
            int64_t delta = _animationState.targetTimelineRange - _timelineRange;
            if ( std::abs( delta ) < 10 )
            {
               _timelineRange = _animationState.targetTimelineRange;
            }
            else
            {
               _timelineRange += delta / speedFactor;
            }
         }

         if (std::abs(_verticalPosPxl - _animationState.targetVerticalPosPxl) > 0.00001f)
         {
            float delta = _animationState.targetVerticalPosPxl - _verticalPosPxl;
            if (std::abs(delta) < 0.01f)
            {
               _verticalPosPxl = _animationState.targetVerticalPosPxl;
            }
            else
            {
               _verticalPosPxl += delta * 0.1;
            }
         }
         break;
      }
   }

   // Update the highlight factor
   static float x = 0.0f;
   x += 0.007f * deltaTimeMs;
   _animationState.highlightPercent = (std::sin( x ) + 1.3f) / 2.0f;

   // Update according to options
   TRACE_HEIGHT = hop::clamp( g_options.traceHeight, MIN_TRACE_HEIGHT, MAX_TRACE_HEIGHT );
   PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

   // Update current lod level
   _lodLevel = 0;
   while ( _lodLevel < LOD_COUNT - 1 && _timelineRange > LOD_NANOS[_lodLevel] )
   {
      ++_lodLevel;
   }
}

void Timeline::draw(
    std::vector<ThreadInfo>& tracesPerThread,
    const StringDb& strDb )
{
   HOP_PROF_FUNC();

   ImGui::BeginChild("TimelineAndCanvas");
   const auto startDrawPos = ImGui::GetCursorScreenPos();
   drawTimeline(startDrawPos.x, startDrawPos.y + 5);

   ImGui::BeginChild(
      "TimelineCanvas",
      ImVec2(0, 0),
      false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove );

   // Set the scroll and get it back from ImGui to have the clamped value
   ImGui::SetScrollY(_verticalPosPxl);

   char threadName[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread " );
   for ( size_t i = 0; i < tracesPerThread.size(); ++i )
   {
      const bool threadHidden = tracesPerThread[i]._hidden;
      snprintf(
          threadName + threadNamePrefix, sizeof( threadName ) - threadNamePrefix, "%lu", i );

      HOP_PROF_DYN_NAME( threadName );

      const auto& zoneColors = g_options.zoneColors;

      uint32_t threadLabelCol = zoneColors[ (i+1) % HOP_MAX_ZONES ];
      if(threadHidden)
         threadLabelCol = DISABLED_COLOR;

      ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, addColorWithClamping( threadLabelCol, HOVERED_COLOR_DELTA ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, addColorWithClamping( threadLabelCol, ACTIVE_COLOR_DELTA ) );
      if ( ImGui::Button( threadName ) )
      {
         tracesPerThread[i]._hidden = !threadHidden;
      }
      else if ( ImGui::IsMouseReleased( 1 ) && ImGui::IsItemHovered() )
      {
         displayModalWindow( "Computing total trace size...", MODAL_TYPE_NO_CLOSE );
         std::thread t( [this, i, dispTrace = tracesPerThread[i]._traces.copy() ]() {
            _traceDetails = createGlobalTraceDetails( dispTrace, i );
            closeModalWindow();
         } );
         t.detach();
      }
      ImGui::PopStyleColor( 3 );
      ImGui::Separator();

      tracesPerThread[i]._localTracesVerticalStartPos= ImGui::GetCursorPosY();
      const float absTracesVerticalStartPos = ImGui::GetCursorScreenPos().y;
      tracesPerThread[i]._absoluteTracesVerticalStartPos = absTracesVerticalStartPos;

      if (!threadHidden)
      {
         ImVec2 curDrawPos = ImGui::GetCursorScreenPos();

         const float threadStartRelDrawPos = curDrawPos.y - ImGui::GetWindowPos().y;
         const float threadEndRelDrawPos =
             threadStartRelDrawPos + ( tracesPerThread[i]._traces.maxDepth * PADDED_TRACE_SIZE ) +
             PADDING_BETWEEN_THREADS;

         const bool tracesVisible =
             !( threadStartRelDrawPos > ImGui::GetWindowHeight() || threadEndRelDrawPos < 0 );

         if( tracesVisible )
         {
            // Draw the lock waits (before traces so that they are not hiding them)
            drawLockWaits(tracesPerThread, i, startDrawPos.x, absTracesVerticalStartPos);
            drawTraces( tracesPerThread[i], i, curDrawPos.x, curDrawPos.y, strDb);
         }


         curDrawPos.y += tracesPerThread[i]._traces.maxDepth * PADDED_TRACE_SIZE + PADDING_BETWEEN_THREADS;
         ImGui::SetCursorScreenPos(curDrawPos);
      }
   }

   // Draw the overlay stuff after having drawn the traces
   // Draw timeline mouse indicator
   if (_timelineHoverPos > 0.0f)
   {
      static char text[32] = {};
      const int64_t hoveredNano = _timelineStart + pxlToNanos(ImGui::GetWindowWidth(), _timelineRange, _timelineHoverPos - startDrawPos.x);
      hop::formatNanosTimepointToDisplay(hoveredNano, _timelineRange, text, sizeof(text));
      drawHoveringTimelineLine(_timelineHoverPos, startDrawPos.y, text);
   }

   if( !_bookmarks.times.empty() )
   {
      const auto& windowSize = ImGui::GetWindowSize();
      ImGui::PushClipRect( ImVec2( startDrawPos.x, startDrawPos.y ), ImVec2( startDrawPos.x + windowSize.x, startDrawPos.y + windowSize.y ), false );
      ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.8f, 1.0f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.0f, 0.0f, 0.9f, 1.0f ));
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.0f, 0.0f, 1.0f, 1.0f ));
      for( auto t : _bookmarks.times )
      {
         float posXPxl = nanosToPxl( windowSize.x, _timelineRange, t - _timelineStart );
         drawBookmarks( posXPxl + startDrawPos.x, startDrawPos.y );
      }
      ImGui::PopClipRect();
      ImGui::PopStyleColor(3);
   }

   if ( _contextMenuInfo.open )
   {
      ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
      ImGui::SetNextWindowBgAlpha(0.8f); // Transparent background
      if( ImGui::BeginPopupContextItem( "Context Menu" ) )
      {
         if ( ImGui::Selectable( "Profile Stack" ) )
         {
            _traceDetails = createTraceDetails(
                tracesPerThread[_contextMenuInfo.threadIndex]._traces,
                _contextMenuInfo.threadIndex,
                _contextMenuInfo.traceId );
            _contextMenuInfo.open = false;
            ImGui::CloseCurrentPopup();
         }
         else if ( ImGui::Selectable("Trace Stats") )
         {
            _traceStats = createTraceStats(
               tracesPerThread[_contextMenuInfo.threadIndex]._traces,
               _contextMenuInfo.threadIndex,
               _contextMenuInfo.traceId);
         }
         ImGui::EndPopup();
      }
      ImGui::PopStyleVar();
   }

   ImGui::EndChild(); // TimelineCanvas

   if (_traceStats.open)
   {
      drawTraceStats(_traceStats, tracesPerThread, strDb);
   }

   if ( ImGui::IsItemHoveredRect() )
   {
      ImVec2 mousePosInCanvas = ImVec2(
          ImGui::GetIO().MousePos.x - startDrawPos.x, ImGui::GetIO().MousePos.y - startDrawPos.y );

      if( ImGui::IsRootWindowOrAnyChildHovered() )
         handleMouseWheel( mousePosInCanvas.x, mousePosInCanvas.y );

      if( ImGui::IsRootWindowOrAnyChildFocused() )
         handleMouseDrag( mousePosInCanvas.x, mousePosInCanvas.y );
   }

   ImGui::EndChild(); // TimelineAndCanvas
}

void Timeline::drawTimeline( const float posX, const float posY )
{
   HOP_PROF_FUNC();

   constexpr float TIMELINE_TOTAL_HEIGHT = 50.0f;
   constexpr uint64_t minStepSize = 10;
   constexpr uint64_t minStepCount = 20;
   constexpr uint64_t maxStepCount = 140;

   const float windowWidthPxl = ImGui::GetWindowWidth();

   ImGui::BeginChild("Timeline", ImVec2( windowWidthPxl, TIMELINE_TOTAL_HEIGHT) );

   const uint64_t stepsCount = [=]() {
      uint64_t stepsCount = _timelineRange / _stepSizeInNanos;
      while ( stepsCount > maxStepCount ||
              ( stepsCount < minStepCount && _stepSizeInNanos > minStepSize ) )
      {
         if ( stepsCount > maxStepCount )
         {
            _stepSizeInNanos *= 5;
         }
         else if ( stepsCount < minStepCount )
         {
            _stepSizeInNanos = std::max( _stepSizeInNanos / 5, minStepSize );
         }
         stepsCount = _timelineRange / _stepSizeInNanos;
      }
      return stepsCount;
   }();

   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   // Draw darker background under the timeline to differentiate with the canvas
   DrawList->AddRectFilled(
      ImVec2(posX, posY),
      ImVec2(posX + windowWidthPxl, posY + TIMELINE_TOTAL_HEIGHT),
      ImColor( 0.1f, 0.1f, 0.1f ));

   // Start drawing the vertical lines on the timeline
   constexpr float smallLineLength = 10.0f;
   constexpr float deltaBigLineLength = 12.0f;  // The diff between the small line and big one
   constexpr float deltaMidLineLength = 7.0f;   // The diff between the small line and mid one

   const float stepSizePxl = nanosToPxl<float>( windowWidthPxl, _timelineRange, _stepSizeInNanos );
   const int64_t stepsDone = _timelineStart / _stepSizeInNanos;
   const int64_t remainder = _timelineStart % _stepSizeInNanos;
   int remainderPxl = 0;
   if ( remainder != 0 ) remainderPxl = nanosToPxl( windowWidthPxl, _timelineRange, remainder );

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top( posX, posY );
   top.x -= ( stepSizePxl + remainderPxl ) - stepSizePxl;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int64_t count = stepsDone;
   std::vector<std::pair<ImVec2, int64_t> > textPos;
   const auto maxPosX = posX + windowWidthPxl;
   for ( double i = top.x; i < maxPosX; i += stepSizePxl, ++count )
   {
      // Draw biggest begin/end lines
      if ( count % 10 == 0 )
      {
         auto startEndLine = bottom;
         startEndLine.y += deltaBigLineLength;
         DrawList->AddLine( top, startEndLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 3.0f );
         textPos.emplace_back(
             ImVec2( startEndLine.x, startEndLine.y + 5.0f ), count * _stepSizeInNanos );
      }
      // Draw midline
      else if ( count % 5 == 0 )
      {
         auto midLine = bottom;
         midLine.y += deltaMidLineLength;
         DrawList->AddLine( top, midLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.5f );
      }
      else
      {
         DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
      }

      top.x += stepSizePxl;
      bottom.x += stepSizePxl;
   }

   // Draw horizontal line
   DrawList->AddLine(
       ImVec2( posX, posY ),
       ImVec2( posX + windowWidthPxl, posY ),
       ImGui::GetColorU32( ImGuiCol_Border ) );

   const int64_t total = stepsCount * _stepSizeInNanos;
   if ( total < 1000 )
   {
      // print as nanoseconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%" PRId64 " ns", pos.second );
      }
   }
   else if ( total < 1000000 )
   {
      // print as microsecs
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f us", (double)( pos.second ) / 1000.0f );
      }
   }
   else if ( total < 1000000000 )
   {
      // print as milliseconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f ms", (double)( pos.second ) / 1000000.0f );
      }
   }
   else if ( total < 1000000000000 )
   {
      // print as seconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f s", (double)( pos.second ) / 1000000000.0f );
      }
   }

   ImGui::EndChild();

   if (ImGui::IsItemHovered())
   {
      const auto curMousePosInScreen = ImGui::GetMousePos();
      _timelineHoverPos = curMousePosInScreen.x;
      if( ImGui::IsMouseClicked( 1 ) )
      {
         _bookmarks.times.push_back( _timelineStart + pxlToNanos(ImGui::GetWindowWidth(), _timelineRange, _timelineHoverPos - posX) );
         std::sort( _bookmarks.times.begin(), _bookmarks.times.end() );
      }
   }
   else
   {
      _timelineHoverPos = -1.0f;
   }

   ImGui::SetCursorScreenPos( ImVec2{posX, posY + TIMELINE_TOTAL_HEIGHT } );
}

void Timeline::handleMouseWheel( float mousePosX, float )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const float mouseWheel = ImGui::GetIO().MouseWheel;

   // Handle vertical scroll
   if( ImGui::IsKeyDown( SDL_SCANCODE_LSHIFT ) )
   {
      const float maxScrollY = maxVerticalPosPxl();
      constexpr float scrollAmount = 50.0f;
      if( mouseWheel > 0)
      {
         moveVerticalPositionPxl(hop::clamp( verticalPosPxl() - scrollAmount, 0.0f, maxScrollY), ANIMATION_TYPE_NONE);
      }
      else if( mouseWheel < 0 )
      {
         moveVerticalPositionPxl(hop::clamp( verticalPosPxl() + scrollAmount, 0.0f, maxScrollY), ANIMATION_TYPE_NONE);
      }
   }
   else // Handle zoom
   {
      if( mouseWheel > 0)
      {
         zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 0.5 : 0.9f );
      }
      else if( mouseWheel < 0 )
      {
         zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 1.5 : 1.1f );
      }
   }
}

void Timeline::handleMouseDrag( float mouseInCanvasX, float mouseInCanvasY )
{
   // Left mouse button dragging
   if ( ImGui::IsMouseDragging( 0 ) )
   {
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const auto delta = ImGui::GetMouseDragDelta();

      // Set horizontal position
      const int64_t deltaXInNanos =
          pxlToNanos<int64_t>( windowWidthPxl, _timelineRange, delta.x );
      setStartTime( _timelineStart - deltaXInNanos, ANIMATION_TYPE_NONE );
   
      const float maxScrollY = maxVerticalPosPxl();

      moveVerticalPositionPxl(hop::clamp(_verticalPosPxl - delta.y, 0.0f, maxScrollY), ANIMATION_TYPE_NONE);

      ImGui::ResetMouseDragDelta();
      setRealtime( false );
   }
   // Ctrl + right mouse dragging
   else if( ImGui::GetIO().KeyCtrl && ImGui::IsMouseDragging( 1 ) )
   {
      ImDrawList* DrawList = ImGui::GetWindowDrawList();
      const auto delta = ImGui::GetMouseDragDelta( 1 );

      const auto curMousePosInScreen = ImGui::GetMousePos();
      DrawList->AddRectFilled(
          ImVec2( curMousePosInScreen.x, 0 ),
          ImVec2( curMousePosInScreen.x - delta.x, 9999 ),
          ImColor( 64, 64, 255, 64 ) );

      // If it is the first time we enter
      if ( _ctrlRightClickStartPosInCanvas[0] == 0.0f )
      {
         _ctrlRightClickStartPosInCanvas[0] = mouseInCanvasX;
         _ctrlRightClickStartPosInCanvas[1] = mouseInCanvasY;
         setRealtime( false );
      }
   }
   // Right mouse button dragging
   else if ( ImGui::IsMouseDragging( 1 ) )
   {
      ImDrawList* DrawList = ImGui::GetWindowDrawList();
      const auto delta = ImGui::GetMouseDragDelta( 1 );

      const auto curMousePosInScreen = ImGui::GetMousePos();
      DrawList->AddRectFilled(
          ImVec2( curMousePosInScreen.x, 0 ),
          ImVec2( curMousePosInScreen.x - delta.x, 9999 ),
          ImColor( 255, 255, 255, 64 ) );

      // If it is the first time we enter
      if ( _rightClickStartPosInCanvas[0] == 0.0f )
      {
         _rightClickStartPosInCanvas[0] = mouseInCanvasX;
         _rightClickStartPosInCanvas[1] = mouseInCanvasY;
         setRealtime( false );
      }
   }

   // Handle right mouse click up. (Finished right click selection zoom)
   if ( ImGui::IsMouseReleased( 1 ) && _rightClickStartPosInCanvas[0] != 0.0f )
   {
      pushNavigationState();

      const float minX = std::min( _rightClickStartPosInCanvas[0], mouseInCanvasX );
      const float maxX = std::max( _rightClickStartPosInCanvas[0], mouseInCanvasX );
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const int64_t minXinNanos =
        pxlToNanos<int64_t>( windowWidthPxl, _timelineRange, minX - 2 );
      setStartTime( _timelineStart + minXinNanos );
      setZoom( pxlToNanos<TimeDuration>( windowWidthPxl, _timelineRange, maxX - minX) );

      // Reset position
      _rightClickStartPosInCanvas[0] = _rightClickStartPosInCanvas[1] = 0.0f;
   }
}

bool Timeline::realtime() const noexcept { return _realtime; }

void Timeline::setRealtime( bool isRealtime ) noexcept
{
   // If we are not realtime and we are going to, push undo
   // state so we can go back to were we were.
   if( !_realtime && isRealtime )
      pushNavigationState();

   _realtime = isRealtime;
}

hop::TimeStamp Timeline::absoluteStartTime() const noexcept
{
   return _absoluteStartTime;
}

hop::TimeStamp Timeline::absolutePresentTime() const noexcept
{
   return _absolutePresentTime;
}

void Timeline::setAbsoluteStartTime( TimeStamp time ) noexcept
{
   _absoluteStartTime = time;
}

void Timeline::setAbsolutePresentTime( TimeStamp time ) noexcept
{
   _absolutePresentTime = time;
}

TimeDuration Timeline::timelineRange() const noexcept { return _timelineRange; }
TimeStamp Timeline::timelineStart() const noexcept { return _timelineStart; }

TimeStamp Timeline::absoluteTimelineStart() const noexcept
{
   return _absoluteStartTime + _timelineStart;
}

TimeStamp Timeline::absoluteTimelineEnd() { return absoluteTimelineStart() + _timelineRange; }

float Timeline::verticalPosPxl() const noexcept
{
   return _verticalPosPxl;
}

float Timeline::maxVerticalPosPxl() const noexcept
{
   // Set vertical position
   // Switch to the traces context to get scroll info
   ImGui::BeginChild( "TimelineCanvas" );
   const float maxScrollY = ImGui::GetScrollMaxY() - ImGui::GetWindowHeight();
   ImGui::EndChild();
   return maxScrollY;
}

int Timeline::currentLodLevel() const noexcept
{
   return _lodLevel;
}

TraceDetails& Timeline::getTraceDetails() noexcept
{
   return _traceDetails;
}

void Timeline::clearTraceDetails()
{
   _traceDetails = TraceDetails{};
}

void Timeline::clearTraceStats()
{
   _traceStats = TraceStats{ 0, 0, 0, 0, 0, std::vector< float >(), false, false };
}

void Timeline::setTraceDetailsDisplayed()
{
    _traceDetails.shouldFocusWindow = false;
}

void Timeline::setStartTime( int64_t time, AnimationType animType ) noexcept
{
   _animationState.targetTimelineStart = time;
   _animationState.type = animType;
   if( animType == ANIMATION_TYPE_NONE )
   {
      // We need to update it immediately as subsequent call might need it updated
      // before the next update
      _timelineStart = time;
   }
}

void Timeline::moveVerticalPositionPxl( float positionPxl, AnimationType animType )
{
   _animationState.targetVerticalPosPxl = positionPxl;
   _animationState.type = animType;
   if (animType == ANIMATION_TYPE_NONE)
   {
      _verticalPosPxl = positionPxl;
   }
}

void Timeline::moveToAbsoluteTime( TimeStamp time, AnimationType animType ) noexcept
{
   moveToTime( time - _absoluteStartTime, animType );
}

void Timeline::moveToTime( int64_t time, AnimationType animType ) noexcept
{
   setStartTime( time - ( _timelineRange * 0.5 ), animType );
}

void Timeline::moveToStart( AnimationType animType ) noexcept
{
   moveToTime( _timelineRange * 0.5f, animType );
   setRealtime( false );
}

void Timeline::moveToPresentTime( AnimationType animType ) noexcept
{
   moveToTime( ( _absolutePresentTime - _absoluteStartTime ), animType );
}

void Timeline::frameToTime( int64_t time, TimeDuration duration ) noexcept
{
   setStartTime( time );
   setZoom( duration );
}

void Timeline::frameToAbsoluteTime( TimeStamp time, TimeDuration duration ) noexcept
{
   frameToTime( time - _absoluteStartTime, duration );
}

void Timeline::setZoom( TimeDuration timelineDuration, AnimationType animType )
{
   _animationState.targetTimelineRange = hop::clamp( timelineDuration, MIN_NANOS_TO_DISPLAY, MAX_NANOS_TO_DISPLAY );
   _animationState.type = animType;
   if( animType == ANIMATION_TYPE_NONE )
   {
      // We need to update it immediately as subsequent call might need it updated
      // before the next update
      _timelineRange = _animationState.targetTimelineRange;
   }
}

void Timeline::zoomOn( int64_t nanoToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t nanoToZoom = nanoToZoomOn - _timelineStart;

   const auto prevTimelineRange = _timelineRange;
   setZoom( _timelineRange * zoomFactor, ANIMATION_TYPE_NONE );

   const int64_t prevPxlPos = nanosToPxl( windowWidthPxl, prevTimelineRange, nanoToZoom );
   const int64_t newPxlPos = nanosToPxl( windowWidthPxl, _timelineRange, nanoToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToNanos( windowWidthPxl, _timelineRange, pxlDiff );
      setStartTime( _timelineStart + timeDiff, ANIMATION_TYPE_NONE );
   }
}

static uint32_t setBitIndex( TZoneId_t zone )
{
   uint32_t count = 0;
   while ( zone )
   {
      zone = zone >> 1;
      ++count;
   }
   return count-1;
}

void Timeline::drawTraces(
    const ThreadInfo& data,
    uint32_t threadIndex,
    const float posX,
    const float posY,
    const StringDb& strDb )
{
   if ( data._traces.ends.empty() ) return;

   HOP_PROF_FUNC();

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   struct DrawingInfo
   {
      ImVec2 posPxl;
      TimeDuration duration;
      size_t traceIndex;
      float lengthPxl;
   };

   static std::array< std::vector< DrawingInfo >, HOP_MAX_ZONES > tracesToDraw;
   static std::array< std::vector< DrawingInfo >, HOP_MAX_ZONES > lodTracesToDraw;
   for( size_t i = 0; i < lodTracesToDraw.size(); ++i )
   {
      tracesToDraw[ i ].clear();
      lodTracesToDraw[ i ].clear();
   }

   static std::vector<DrawingInfo> highlightTraceToDraw;
   highlightTraceToDraw.clear();

   // Find the best lodLevel for our current zoom
   const int lodLevel = currentLodLevel();

   g_stats.currentLOD = lodLevel;

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteTimelineStart();
   const TimeStamp lastTraceAbsoluteTime = absoluteTimelineEnd();

   const auto span = visibleIndexSpan(
       data._traces.lods, lodLevel, firstTraceAbsoluteTime, lastTraceAbsoluteTime, 0 );

   if( span.first == hop::INVALID_IDX ) return;

   for ( size_t i = span.first; i < span.second; ++i )
   {
      const auto& t = data._traces.lods[lodLevel][i];
      const TimeStamp traceEndTime = ( t.end - absoluteStart );
      const auto traceEndPxl = nanosToPxl<float>(
          windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
      const float traceLengthPxl = std::max(
          MIN_TRACE_LENGTH_PXL, nanosToPxl<float>( windowWidthPxl, _timelineRange, t.delta ) );

      const auto tracePos = ImVec2(
          posX + traceEndPxl - traceLengthPxl,
          posY + t.depth * PADDED_TRACE_SIZE);
      const uint32_t zoneIndex = setBitIndex(data._traces.zones[t.traceIndex]);
      if ( t.isLoded )
      {
         lodTracesToDraw[ zoneIndex ].push_back(
             DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
      }
      else
      {
         tracesToDraw[ zoneIndex ].push_back(
             DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
         for( const auto& tid : _highlightedTraces )
         {
            if( threadIndex == tid.second && t.traceIndex == tid.first )
            {
               highlightTraceToDraw.push_back( DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
            }
         }
      }
   }

   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;

   // Draw the loded traces
   char curName[512] = "<Multiple Elements> ~";
   const size_t hoveredNamePrefixSize = strlen( curName );
   for ( size_t zoneId = 0; zoneId < lodTracesToDraw.size(); ++zoneId )
   {
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[zoneId], HOVERED_COLOR_DELTA ) );
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[zoneId], ACTIVE_COLOR_DELTA ) );
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      const auto& traces = lodTracesToDraw[ zoneId ];
      for( const auto& t : traces )
      {
         ImGui::SetCursorScreenPos( t.posPxl );

         ImGui::Button( "", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );

         if ( ImGui::IsItemHovered() )
         {
            if ( t.lengthPxl > 3 )
            {
               ImGui::BeginTooltip();
               formatNanosDurationToDisplay( t.duration, curName + hoveredNamePrefixSize, sizeof( curName ) - hoveredNamePrefixSize );
               ImGui::TextUnformatted( curName );
               ImGui::EndTooltip();
            }

            if ( leftMouseDblClicked )
            {
               pushNavigationState();
               const TimeStamp traceEndTime =
                   pxlToNanos( windowWidthPxl, _timelineRange, t.posPxl.x - posX + t.lengthPxl );
               frameToTime( _timelineStart + ( traceEndTime - t.duration ), t.duration );
            }
            else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
            {
               ImGui::OpenPopup( "Context Menu" );
               _contextMenuInfo.open = true;
               _contextMenuInfo.threadIndex = threadIndex;
               _contextMenuInfo.traceId = t.traceIndex;
            }
         }
      }
      ImGui::PopStyleColor( 3 );
      ImGui::PopStyleVar();
   }

   char formattedTime[64] = {};
   // Draw the non-loded traces
   for ( size_t zoneId = 0; zoneId < tracesToDraw.size(); ++zoneId )
   {
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[zoneId], HOVERED_COLOR_DELTA ) );
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[zoneId], ACTIVE_COLOR_DELTA ) );
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      const auto& traces = tracesToDraw[ zoneId ];
      for( const auto& t : traces )
      {
         const size_t traceIndex = t.traceIndex;
         snprintf( curName, sizeof(curName), "%s", strDb.getString( data._traces.fctNameIds[traceIndex] ) );

         ImGui::SetCursorScreenPos( t.posPxl );
         ImGui::Button( curName, ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
         if ( ImGui::IsItemHovered() )
         {
            if ( t.lengthPxl > 3 )
            {
               size_t lastChar = strlen( curName );
               curName[lastChar] = ' ';
               ImGui::BeginTooltip();
               formatNanosDurationToDisplay( t.duration, formattedTime, sizeof( formattedTime ) );
               snprintf(
                   curName + lastChar,
                   sizeof( curName ) - lastChar,
                   " (%s)\n   %s:%d ",
                   formattedTime,
                   strDb.getString( data._traces.fileNameIds[traceIndex] ),
                   data._traces.lineNbs[traceIndex] );
               ImGui::TextUnformatted( curName );
               ImGui::EndTooltip();
            }

            if ( leftMouseDblClicked )
            {
               setZoom( t.duration );
               setStartTime(
                   ( data._traces.ends[traceIndex] - data._traces.deltas[traceIndex] - absoluteStart ) );
            }
            else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f )
            {
               ImGui::OpenPopup( "Context Menu" );
               _contextMenuInfo.open = true;
               _contextMenuInfo.threadIndex = threadIndex;
               _contextMenuInfo.traceId = t.traceIndex;
            }
         }
      }
      ImGui::PopStyleColor( 3 );
      ImGui::PopStyleVar();
   }

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 1.0f, 1.0f, 1.0f, 0.5f * _animationState.highlightPercent ).Value );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ).Value );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ).Value );
   for( const auto& t : highlightTraceToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
   }
   ImGui::PopStyleColor( 3 );
}

namespace
{
   struct unlock_events_less_cmp
   {
      bool operator()(const hop::UnlockEvent& ue, hop::TimeStamp time)
      {
         return ue.time < time;
      }

      bool operator()(hop::TimeStamp time, const hop::UnlockEvent& ue)
      {
         return time < ue.time;
      }
   };
}

std::vector< Timeline::LockOwnerInfo > Timeline::highlightLockOwner(
    const std::vector<ThreadInfo>& infos,
    uint32_t threadIndex,
    uint32_t hoveredLwIndex,
    const float posX,
    const float /*posY*/ )
{
    HOP_PROF_FUNC();
    std::vector< LockOwnerInfo > lockInfos;
    lockInfos.reserve( 16 );

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const auto absoluteStart = _absoluteStartTime;
    const int highlightAlpha = 70.0f * _animationState.highlightPercent;

    const void* highlightedMutexAddr = infos[threadIndex]._lockWaits.mutexAddrs[hoveredLwIndex];
    const TimeDuration highlightedLWDelta = infos[threadIndex]._lockWaits.deltas[hoveredLwIndex];
    const TimeStamp highlightedLWEndTime = infos[threadIndex]._lockWaits.ends[hoveredLwIndex];
    const TimeStamp highlightedLWStartTime = highlightedLWEndTime - highlightedLWDelta;
    for (size_t i = 0; i < infos.size(); ++i)
    {
        if (i == threadIndex || infos[i]._hidden) continue;

        const DisplayableLockWaits& lockWaits = infos[i]._lockWaits;

        const float startNanosAsPxl =
           nanosToPxl<float>(windowWidthPxl, _timelineRange, _timelineStart);

        auto lastUnlock = std::lower_bound(
            infos[i]._unlockEvents.cbegin(),
            infos[i]._unlockEvents.cend(),
            highlightedLWEndTime,
            unlock_events_less_cmp() );

        // lower_bound returns the first that is not smaller. We need the one just before that
        if(lastUnlock != infos[i]._unlockEvents.cbegin() ) --lastUnlock;

        while( lastUnlock != infos[i]._unlockEvents.cbegin() )
        {
            if(lastUnlock->mutexAddress == highlightedMutexAddr )
            {
               // We've gone to far, so early break
               if(lastUnlock->time < highlightedLWStartTime )
                  break;

               // Find the associated lock wait
               const auto lockWaitIt = std::lower_bound(
                   lockWaits.ends.cbegin(), lockWaits.ends.cend(), lastUnlock->time );
               size_t lockWaitIdx = std::distance( lockWaits.ends.begin(), lockWaitIt );

               // lower_bound returns the first that does not compare smaller than the unlock time.
               // Therefore, we need to start from this iterator and find the first one that matches
               // the highlighted mutex
               if( lockWaitIdx > 0 ) --lockWaitIdx;

               while ( lockWaitIdx > 0 &&
                       lockWaits.mutexAddrs[lockWaitIdx] != highlightedMutexAddr )
               {
                  --lockWaitIdx;
               }

               const TimeStamp lockWaitEndTime = lockWaits.ends[lockWaitIdx];
               // Add info to result vector
               bool added = false;
               for( auto& info : lockInfos )
               {
                  if( info.threadIndex == i )
                  {
                     info.lockDuration += lastUnlock->time - lockWaitEndTime;
                     added = true;
                     break;
                  }
               }
               if( !added )
                  lockInfos.emplace_back( lastUnlock->time - lockWaitEndTime, i );

               const int64_t lockTimeAsPxl = nanosToPxl<float>(
                  windowWidthPxl,
                  _timelineRange,
                  (lockWaitEndTime - absoluteStart));
               const int64_t unlockTimeAsPxl = nanosToPxl<float>(
                  windowWidthPxl, _timelineRange, (lastUnlock->time - absoluteStart));

               const float tracesHeight = (infos[i]._traces.maxDepth + 1) * Timeline::PADDED_TRACE_SIZE;

               DrawList->AddRectFilled(
                  ImVec2(posX - startNanosAsPxl + lockTimeAsPxl, infos[i]._absoluteTracesVerticalStartPos),
                  ImVec2(
                     posX - startNanosAsPxl + unlockTimeAsPxl,
                     infos[i]._absoluteTracesVerticalStartPos + tracesHeight),
                  ImColor(0, 255, 0, 30 + highlightAlpha));
            }

            --lastUnlock;
        }
    }

    return lockInfos;
}

void Timeline::drawLockWaits(
    const std::vector<ThreadInfo>& infos,
    uint32_t threadIndex,
    const float posX,
    const float posY )
{
   const auto& data = infos[threadIndex];
   const DisplayableLockWaits& lockWaits = data._lockWaits;
   if ( lockWaits.ends.empty() ) return;

   HOP_PROF_FUNC();

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + _timelineStart;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + _timelineRange;

   // Find the best lodLevel for our current zoom
   const int lodLevel = currentLodLevel();

   struct DrawingInfo
   {
      ImVec2 posPxl;
      TimeDuration duration;
      size_t traceIndex;
      float lengthPxl;
   };

   const auto span =
       visibleIndexSpan( lockWaits.lods, lodLevel, firstTraceAbsoluteTime, lastTraceAbsoluteTime, 1 );

   if ( span.first == hop::INVALID_IDX ) return;

   static std::vector<DrawingInfo> tracesToDraw, lodTracesToDraw;
   tracesToDraw.clear();
   lodTracesToDraw.clear();

   HOP_PROF_SPLIT( "Gathering drawing info" );

   for ( size_t i = span.first; i < span.second; ++i )
   {
      const auto& t = lockWaits.lods[lodLevel][i];
      const TimeStamp traceEndTime = ( t.end - absoluteStart );
      const auto traceEndPxl =
          nanosToPxl<float>( windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
      const float traceLengthPxl = std::max(
          MIN_TRACE_LENGTH_PXL, nanosToPxl<float>( windowWidthPxl, _timelineRange, t.delta ) );

      const auto tracePos =
          ImVec2( posX + traceEndPxl - traceLengthPxl, posY + t.depth * PADDED_TRACE_SIZE );
      if ( t.isLoded )
      {
         lodTracesToDraw.push_back( DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
      }
      else
      {
         tracesToDraw.push_back( DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
      }
   }

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;
   ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[HOP_MAX_ZONES] );
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[HOP_MAX_ZONES], HOVERED_COLOR_DELTA ) );
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[HOP_MAX_ZONES], ACTIVE_COLOR_DELTA ) );
   ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[HOP_MAX_ZONES] ? 1.0f : disabledZoneOpacity );

   HOP_PROF_SPLIT( "drawing lod" );
   for ( const auto& t : lodTracesToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
      if ( ImGui::IsItemHovered() )
      {
         const auto lockInfo = highlightLockOwner( infos, threadIndex, t.traceIndex, posX, posY );
         (void)lockInfo.empty();
         if ( t.lengthPxl > 3 )
         {
            char lockTooltip[256] = "Waiting lock for ~";
            ImGui::BeginTooltip();
            formatNanosDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ) );

            ImGui::TextUnformatted( lockTooltip );
            ImGui::EndTooltip();
         }

         if ( ImGui::IsMouseDoubleClicked( 0 ) )
         {
            pushNavigationState();
            const TimeDuration delta = lockWaits.deltas[t.traceIndex];
            frameToAbsoluteTime( lockWaits.ends[t.traceIndex] - delta, delta );
         }
      }
   }

   HOP_PROF_SPLIT( "drawing non-lod" );
   for ( const auto& t : tracesToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "Waiting lock...", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
      if ( ImGui::IsItemHovered() )
      {
         const auto lockInfo = highlightLockOwner( infos, threadIndex, t.traceIndex, posX, posY );
         (void)lockInfo.empty();
         if ( t.lengthPxl > 3 )
         {
            char lockTooltip[256] = "Waiting lock for ";
            ImGui::BeginTooltip();
            formatNanosDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ) );

            if ( lockInfo.empty() )
            {
               // Set a message to warn the user than the thread owning the lock is not part of
               // any profiled code
               snprintf(
                   lockTooltip + strlen( lockTooltip ),
                   sizeof( lockTooltip ) - strlen( lockTooltip ),
                   "\n  Threads owning the lock were not profiled" );
            }
            else
            {
               // Print infos about which threads own the lock
               char formattedLockTime[64] = {};
               for ( const auto& i : lockInfo )
               {
                  formatNanosDurationToDisplay(
                      i.lockDuration, formattedLockTime, sizeof( formattedLockTime ) );
                  snprintf(
                      lockTooltip + strlen( lockTooltip ),
                      sizeof( lockTooltip ) - strlen( lockTooltip ),
                      "\n  Thread #%u (%s)",
                      i.threadIndex,
                      formattedLockTime );
               }
            }
            ImGui::TextUnformatted( lockTooltip );
            ImGui::EndTooltip();
         }

         if ( ImGui::IsMouseDoubleClicked( 0 ) )
         {
            pushNavigationState();
            const TimeDuration delta = lockWaits.deltas[t.traceIndex];
            frameToAbsoluteTime( lockWaits.ends[t.traceIndex] - delta, delta );
         }
      }
   }

   ImGui::PopStyleColor( 3 );
   ImGui::PopStyleVar();
}

void Timeline::addTraceToHighlight( const std::pair< size_t, uint32_t >& trace )
{
   _highlightedTraces.push_back( trace );
}

void Timeline::clearHighlightedTraces()
{
   _highlightedTraces.clear();
}

void Timeline::nextBookmark() noexcept
{
   const TimeStamp timelineCenter = _timelineStart + _timelineRange / 2;
   const auto delta = std::abs( _timelineRange * 0.01 );
   auto it = _bookmarks.times.begin();
   while( it != _bookmarks.times.end() )
   {
      if( *it - delta > timelineCenter )
      {
         moveToTime( *it, ANIMATION_TYPE_FAST );
         return;
      }
      ++it;
   }
}

void Timeline::previousBookmark() noexcept
{
   const TimeStamp timelineCenter = _timelineStart + _timelineRange / 2;
   const auto delta = std::abs( _timelineRange * 0.01 );
   auto it = _bookmarks.times.rbegin();
   while( it != _bookmarks.times.rend() )
   {
      if( ( *it + delta < timelineCenter ) )
      {
         moveToTime( *it, ANIMATION_TYPE_FAST );
         return;
      }
      ++it;
   }
}

void Timeline::clearBookmarks()
{
   _bookmarks.times.clear();
}

void Timeline::pushNavigationState() noexcept
{
   _redoPositionStates.clear();
   AnimationState animState = _animationState;
   animState.highlightPercent = 0.0f;
   animState.type = ANIMATION_TYPE_FAST;
   _undoPositionStates.push_back( animState );
}

void Timeline::undoNavigation() noexcept
{
   if( !_undoPositionStates.empty() )
   {
      _redoPositionStates.push_back( _animationState );
      const auto& state = _undoPositionStates.back();
      _animationState.targetTimelineStart = state.targetTimelineStart;
      _animationState.targetTimelineRange = state.targetTimelineRange;
      _animationState.targetVerticalPosPxl = state.targetVerticalPosPxl;
      _animationState.type = ANIMATION_TYPE_FAST;
      _undoPositionStates.pop_back();
   }
}

void Timeline::redoNavigation() noexcept
{
   if( !_redoPositionStates.empty() )
   {
      _undoPositionStates.push_back( _animationState );
      const auto& state = _redoPositionStates.back();
      _animationState.targetTimelineStart = state.targetTimelineStart;
      _animationState.targetTimelineRange = state.targetTimelineRange;
      _animationState.targetVerticalPosPxl = state.targetVerticalPosPxl;
      _animationState.type = ANIMATION_TYPE_FAST;
      _redoPositionStates.pop_back();
   }
}

size_t serializedSize( const Timeline& timeline )
{
   return sizeof( size_t ) + sizeof( TimeStamp ) * timeline._bookmarks.times.size();
}

size_t serialize( const Timeline& timeline, char* data )
{
   const size_t bookmarkCount = timeline._bookmarks.times.size();
   const size_t bookmarksSize = sizeof( TimeStamp ) * bookmarkCount;
   memcpy( &data[0], &bookmarkCount, sizeof( size_t ) );
   memcpy( &data[0] + sizeof( size_t ), timeline._bookmarks.times.data(), bookmarksSize );
   return serializedSize(timeline);
}

size_t deserialize( const char* data, Timeline& timeline )
{
   size_t i = 0;
   const size_t* bookmarkCount = (size_t*)&data[i];
   const size_t bookmarksSize = (*bookmarkCount) * sizeof( TimeStamp );
   i += sizeof( size_t );
   timeline._bookmarks.times.assign( (TimeStamp*)&data[i], (TimeStamp*)(&data[i] + bookmarksSize ) );
   return bookmarksSize + sizeof( size_t );
}

} // namespace hop
