#include "Timeline.h"
#include "TimelineTrack.h"
#include "Utils.h"
#include "ModalWindow.h"
#include "Stats.h"
#include "StringDb.h"
#include "TraceDetail.h"
#include "Options.h"
#include "Utils.h"
#include "Cursor.h"

#include "imgui/imgui.h"

#include <SDL_keycode.h>

#include <thread>
#include <cmath>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static constexpr hop::TimeDuration MIN_NANOS_TO_DISPLAY = 500;
static constexpr hop::TimeDuration MAX_NANOS_TO_DISPLAY = 900000000000;
static constexpr float TIMELINE_TOTAL_HEIGHT = 50.0f;

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

void Timeline::update( float deltaTimeMs ) noexcept
{
   HOP_PROF_FUNC();

   switch ( _animationState.type )
   {
      case ANIMATION_TYPE_NONE:
         _timelineStart = _animationState.targetTimelineStart;
         _duration = _animationState.targetTimelineRange;
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

         if ( std::abs( _duration - _animationState.targetTimelineRange ) > 0.00001 )
         {
            int64_t delta = _animationState.targetTimelineRange - _duration;
            if ( std::abs( delta ) < 10 )
            {
               _duration = _animationState.targetTimelineRange;
            }
            else
            {
               _duration += delta / speedFactor;
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
}

void Timeline::draw( float timelineHeight )
{
   HOP_PROF_FUNC();

   //ImGui::BeginChild("TimelineAndCanvas");
   const auto startDrawPos = ImGui::GetCursorScreenPos();
   drawTimeline(startDrawPos.x, startDrawPos.y + 5);

   // Save the canvas draw position for later
   const auto& curDrawPos = ImGui::GetCursorScreenPos();
   _canvasDrawPosition[0] = curDrawPos.x;
   _canvasDrawPosition[1] = curDrawPos.y;

   ImGui::BeginChild(
      "TimelineCanvas",
      ImVec2(0,0),
      false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove );

   // Set the scroll and get it back from ImGui to have the clamped value
   ImGui::SetScrollY(_verticalPosPxl);

   // Draw the overlay stuff after having drawn the traces
   // Draw timeline mouse indicator
   if (_timelineHoverPos > 0.0f)
   {
      static char text[32] = {};
      const int64_t hoveredNano = _timelineStart + pxlToNanos(ImGui::GetWindowWidth(), _duration, _timelineHoverPos - startDrawPos.x);
      hop::formatNanosTimepointToDisplay(hoveredNano, _duration, text, sizeof(text));
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
         float posXPxl = nanosToPxl( windowSize.x, _duration, t - _timelineStart );
         drawBookmarks( posXPxl + startDrawPos.x, startDrawPos.y );
      }
      ImGui::PopClipRect();
      ImGui::PopStyleColor(3);
   }

   if ( _contextMenuInfo.open )
   {
      // ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
      // ImGui::SetNextWindowBgAlpha(0.8f); // Transparent background
      // if( ImGui::BeginPopupContextItem( "Context Menu" ) )
      // {
      //    if ( ImGui::Selectable( "Trace Stats" ) )
      //    {
      //       _traceStats = createTraceStats(
      //          tracesPerThread[_contextMenuInfo.threadIndex]._traces,
      //          _contextMenuInfo.threadIndex,
      //          _contextMenuInfo.traceId);
      //    }
      //    else if ( ImGui::Selectable( "Profile Stack" ) )
      //    {
      //       _traceDetails = createTraceDetails(
      //           tracesPerThread[_contextMenuInfo.threadIndex]._traces,
      //           _contextMenuInfo.threadIndex,
      //           _contextMenuInfo.traceId );
      //       _contextMenuInfo.open = false;
      //       ImGui::CloseCurrentPopup();
      //    }
      //    else if( ImGui::Selectable( "Profile Track" ) )
      //    {
      //       displayModalWindow( "Computing total trace size...", MODAL_TYPE_NO_CLOSE );
      //       const uint32_t tIdx = _contextMenuInfo.threadIndex;
      //       std::thread t( [this, tIdx, dispTrace = tracesPerThread[tIdx]._traces.copy()]() {
      //          _traceDetails = createGlobalTraceDetails( dispTrace, tIdx );
      //          closeModalWindow();
      //       } );
      //       t.detach();
      //    }
      //    else if( ImGui::Selectable( "Resize Tracks to Fit" ) )
      //    {
      //       //resizeAllTracksToFit( tracesPerThread );
      //    }
      //    ImGui::EndPopup();
      // }
      // ImGui::PopStyleVar();
   }

   // Draw an invislbe button to extend the child region to allow scrolling
   ImGui::SetCursorScreenPos( ImVec2( 0.0f, timelineHeight ) );
   ImGui::InvisibleButton( "ExtendRegion", ImVec2( 0.0f, 0.0f ) );

   ImGui::EndChild(); // TimelineCanvas

   // if (_traceStats.open)
   // {
   //    drawTraceStats(_traceStats, tracesPerThread, strDb);
   // }

   //if ( ImGui::IsItemHoveredRect() )
   //{
      ImVec2 mousePosInCanvas = ImVec2(
          ImGui::GetIO().MousePos.x - startDrawPos.x, ImGui::GetIO().MousePos.y - startDrawPos.y );

      //if( ImGui::IsRootWindowOrAnyChildHovered() )
         handleMouseWheel( mousePosInCanvas.x, mousePosInCanvas.y );

      //if( ImGui::IsRootWindowOrAnyChildFocused() )
         handleMouseDrag( mousePosInCanvas.x, mousePosInCanvas.y );
   //}
}

void Timeline::drawTimeline( const float posX, const float posY )
{
   HOP_PROF_FUNC();

   constexpr uint64_t minStepSize = 10;
   constexpr uint64_t minStepCount = 20;
   constexpr uint64_t maxStepCount = 140;

   const float windowWidthPxl = ImGui::GetWindowWidth();

   ImGui::BeginChild("Timeline", ImVec2( windowWidthPxl, TIMELINE_TOTAL_HEIGHT) );

   const uint64_t stepsCount = [=]() {
      uint64_t stepsCount = _duration / _stepSizeInNanos;
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
         stepsCount = _duration / _stepSizeInNanos;
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

   const float stepSizePxl = nanosToPxl<float>( windowWidthPxl, _duration, _stepSizeInNanos );
   const int64_t stepsDone = _timelineStart / _stepSizeInNanos;
   const int64_t remainder = _timelineStart % _stepSizeInNanos;
   int remainderPxl = 0;
   if ( remainder != 0 ) remainderPxl = nanosToPxl( windowWidthPxl, _duration, remainder );

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
         _bookmarks.times.push_back( _timelineStart + pxlToNanos(ImGui::GetWindowWidth(), _duration, _timelineHoverPos - posX) );
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
         zoomOn( pxlToNanos( windowWidthPxl, _duration, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 0.5 : 0.9f );
      }
      else if( mouseWheel < 0 )
      {
         zoomOn( pxlToNanos( windowWidthPxl, _duration, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 1.5 : 1.1f );
      }
   }
}

void Timeline::handleMouseDrag( float mouseInCanvasX, float mouseInCanvasY )
{
   // Left mouse button dragging
   if ( ImGui::IsMouseDragging( 0 ) )
   {
      // Handle track resize
     // if( _draggedTrack > 0 )
    //  {
         // Find the previous track that is visible
         // int i = _draggedTrack-1;
         // while( i > 0 && tracesPerThread[i].empty() ) {
         //    --i;
         // }

         // const float trackHeight =
         //     ( ImGui::GetMousePos().y - tracesPerThread[i]._absoluteTracesVerticalStartPos - THREAD_LABEL_HEIGHT ) /
         //     TimelineTrack::PADDED_TRACE_SIZE;
         // tracesPerThread[i].setTrackHeight( trackHeight );
     // }
     // else // handle timeline panning
     // {
         const float windowWidthPxl = ImGui::GetWindowWidth();
         const auto delta = ImGui::GetMouseDragDelta();

         // Set horizontal position
         const int64_t deltaXInNanos =
             pxlToNanos<int64_t>( windowWidthPxl, _duration, delta.x );
         setStartTime( _timelineStart - deltaXInNanos, ANIMATION_TYPE_NONE );
      
         const float maxScrollY = maxVerticalPosPxl();

         moveVerticalPositionPxl(hop::clamp(_verticalPosPxl - delta.y, 0.0f, maxScrollY), ANIMATION_TYPE_NONE);

         ImGui::ResetMouseDragDelta();
         setRealtime( false );
 //     }
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
        pxlToNanos<int64_t>( windowWidthPxl, _duration, minX - 2 );
      setStartTime( _timelineStart + minXinNanos );
      setZoom( pxlToNanos<TimeDuration>( windowWidthPxl, _duration, maxX - minX) );

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

hop::TimeStamp Timeline::globalStartTime() const noexcept
{
   return _globalStartTime;
}

hop::TimeStamp Timeline::globalEndTime() const noexcept
{
   return _globalEndTime;
}

void Timeline::setGlobalStartTime( TimeStamp time ) noexcept
{
   _globalStartTime = time;
}

void Timeline::setGlobalEndTime( TimeStamp time ) noexcept
{
   _globalEndTime = time;
}

TimeStamp Timeline::relativeStartTime() const noexcept { return _timelineStart; }

TimeStamp Timeline::relativeEndTime() const noexcept { return _timelineStart + _duration; }

TimeStamp Timeline::absoluteStartTime() const noexcept
{
   return _globalStartTime + _timelineStart;
}

TimeStamp Timeline::absoluteEndTime() const noexcept
{
   return absoluteStartTime() + _duration;
}

TimeDuration Timeline::duration() const noexcept { return _duration; }

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

float Timeline::canvasPosX() const noexcept
{
   return _canvasDrawPosition[0];
}

float Timeline::canvasPosY() const noexcept
{
   return _canvasDrawPosition[1];
}

float Timeline::canvasPosWithScrollX() const noexcept
{
   return _canvasDrawPosition[0] - _verticalPosPxl;
}

float Timeline::canvasPosWithScrollY() const noexcept
{
   return _canvasDrawPosition[1] - _verticalPosPxl;
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
   moveToTime( time - _globalStartTime, animType );
}

void Timeline::moveToTime( int64_t time, AnimationType animType ) noexcept
{
   setStartTime( time - ( _duration * 0.5 ), animType );
}

void Timeline::moveToStart( AnimationType animType ) noexcept
{
   moveToTime( _duration * 0.5f, animType );
   setRealtime( false );
}

void Timeline::moveToPresentTime( AnimationType animType ) noexcept
{
   moveToTime( ( _globalEndTime - _globalStartTime ), animType );
}

void Timeline::frameToTime( int64_t time, TimeDuration duration, bool pushNavState ) noexcept
{
   if( pushNavState ) pushNavigationState();

   setStartTime( time );
   setZoom( duration );
}

void Timeline::frameToAbsoluteTime( TimeStamp time, TimeDuration duration, bool pushNavState ) noexcept
{
   frameToTime( time - _globalStartTime, duration, pushNavState );
}

void Timeline::setZoom( TimeDuration timelineDuration, AnimationType animType )
{
   _animationState.targetTimelineRange = hop::clamp( timelineDuration, MIN_NANOS_TO_DISPLAY, MAX_NANOS_TO_DISPLAY );
   _animationState.type = animType;
   if( animType == ANIMATION_TYPE_NONE )
   {
      // We need to update it immediately as subsequent call might need it updated
      // before the next update
      _duration = _animationState.targetTimelineRange;
   }
}

void Timeline::zoomOn( int64_t nanoToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t nanoToZoom = nanoToZoomOn - _timelineStart;

   const auto prevTimelineRange = _duration;
   setZoom( _duration * zoomFactor, ANIMATION_TYPE_NONE );

   const int64_t prevPxlPos = nanosToPxl( windowWidthPxl, prevTimelineRange, nanoToZoom );
   const int64_t newPxlPos = nanosToPxl( windowWidthPxl, _duration, nanoToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToNanos( windowWidthPxl, _duration, pxlDiff );
      setStartTime( _timelineStart + timeDiff, ANIMATION_TYPE_NONE );
   }
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

// void Timeline::addTraceToHighlight( const std::pair< size_t, uint32_t >& trace )
// {
//    _highlightedTraces.push_back( trace );
// }

// void Timeline::clearHighlightedTraces()
// {
//    _highlightedTraces.clear();
// }

void Timeline::nextBookmark() noexcept
{
   const TimeStamp timelineCenter = _timelineStart + _duration / 2;
   const auto delta = std::abs( _duration * 0.01 );
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
   const TimeStamp timelineCenter = _timelineStart + _duration / 2;
   const auto delta = std::abs( _duration * 0.01 );
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
