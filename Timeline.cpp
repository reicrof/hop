#include "Timeline.h"
#include "TimelineTrack.h"
#include "Utils.h"
#include "StringDb.h"
#include "Options.h"
#include "Cursor.h"

#include "imgui/imgui.h"

#include <SDL_keycode.h>

#include <thread>
#include <cmath>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cstdlib> // fix for std::abs for older libc++ impl

static constexpr hop::TimeDuration MIN_CYCLES_TO_DISPLAY = 1000;
static constexpr hop::TimeDuration MAX_CYCLES_TO_DISPLAY = 1800000000000;
static constexpr float TIMELINE_TOTAL_HEIGHT = 50.0f;

using TimelineTextPositions = std::vector<std::pair<ImVec2, int64_t> >;

static void drawHoveringTimelineLine(float posInScreenX, float timelineStartPosY, const char* text )
{
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
   constexpr float BOOKMARK_WIDTH = 8.0f;
   constexpr float BOOKMARK_HEIGHT = 20.0f;
   constexpr float LINE_PADDING = 5.0f;
   ImGui::SetCursorScreenPos( ImVec2( posXPxl - (BOOKMARK_WIDTH * 0.5f), posYPxl + LINE_PADDING ) );
   ImGui::Button("", ImVec2( BOOKMARK_WIDTH, BOOKMARK_HEIGHT ) );
}

static void drawTextPositionsCycles( const TimelineTextPositions& textPos )
{
   for( const auto& pos : textPos )
   {
      ImGui::SetCursorScreenPos( pos.first );
      if( pos.second >= 1000000 )
      {
         ImGui::Text( "%" PRId64 "k cycles", pos.second / 1000 );
      }
      else
      {
         ImGui::Text( "%" PRId64 " cycles", pos.second );  
      }
   }
}

static void drawTextPositionsTime( const TimelineTextPositions& textPos, uint64_t tlDuration )
{
   const uint64_t tlDurationNs = hop::cyclesToNanos( tlDuration );
   if ( tlDurationNs < 1000 )
   {
      // print as nanoseconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%" PRId64 " ns", hop::cyclesToNanos( pos.second ) );
      }
   }
   else if ( tlDurationNs < 1000000 )
   {
      // print as microsecs
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f us", (double)(  hop::cyclesToNanos( pos.second ) ) / 1000.0f );
      }
   }
   else if ( tlDurationNs < 1000000000 )
   {
      // print as milliseconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f ms", (double)(  hop::cyclesToNanos( pos.second ) ) / 1000000.0f );
      }
   }
   else if ( tlDurationNs < 1000000000000 )
   {
      // print as seconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f s", (double)(  hop::cyclesToNanos( pos.second ) ) / 1000000000.0f );
      }
   }
}

static void drawRangeSelection( float fromPxl, float toPxl, float heightPos, const char* durationText )
{
   static const float TEXT_X_PADDING = 10.0f;
   static const float TEXT_BG_PADDING = 2.0f;
   const ImVec2 textSize = ImGui::CalcTextSize( durationText );
   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddRectFilled(
       ImVec2( fromPxl, 0 ),
       ImVec2( toPxl, 9999 ),
       ImColor( 255, 159, 0, 96 ) );

   // Clamp the label pos inside the selected range
   float labelEnd = std::min( toPxl, ImGui::GetWindowWidth() );
   labelEnd = std::max( fromPxl + textSize.x + TEXT_X_PADDING + TEXT_BG_PADDING, labelEnd );

   const ImVec2 textPos( labelEnd - textSize.x - TEXT_X_PADDING, heightPos + textSize.y );
   drawList->AddRectFilled( ImVec2(textPos.x - TEXT_BG_PADDING, textPos.y - TEXT_BG_PADDING), ImVec2(textPos.x + textSize.x + TEXT_BG_PADDING, textPos.y + textSize.y + TEXT_BG_PADDING), 0X7F000000, 0.2f );
   drawList->AddText( textPos, ImColor(255,255,255), durationText );
}

namespace hop
{

void Timeline::update( float deltaTimeMs ) noexcept
{
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

         const int64_t timelineStartDelta = _animationState.targetTimelineStart - _timelineStart;
         if ( std::abs( timelineStartDelta ) < 10 )
         {
            _timelineStart = _animationState.targetTimelineStart;
         }
         else
         {
            _timelineStart += timelineStartDelta / speedFactor;
         }

         const int64_t timelineDurationDelta = _animationState.targetTimelineRange - _duration;
         if ( std::abs( timelineDurationDelta ) < 10 )
         {
            _duration = _animationState.targetTimelineRange;
         }
         else
         {
            _duration += timelineDurationDelta / speedFactor;
         }

         const float verticalDelta = _animationState.targetVerticalPosPxl - _verticalPosPxl;
         if ( std::abs( verticalDelta ) < 0.01f )
         {
            _verticalPosPxl = _animationState.targetVerticalPosPxl;
         }
         else
         {
            _verticalPosPxl += verticalDelta * 0.1;
         }
         break;
      }
   }
}

void Timeline::draw( float posX, float posY )
{
   HOP_PROF_FUNC();

   _timelineDrawPosition[0] = posX;
   _timelineDrawPosition[1] = posY;

   // Draw the time ruler at the top of the window
   drawTimeline( posX, posY );

   // Save the position once the ruler is drawn. This will be the start of the
   // canvas draw position
   const auto& curDrawPos = ImGui::GetCursorScreenPos();
   _canvasDrawPosition[0] = curDrawPos.x;
   _canvasDrawPosition[1] = curDrawPos.y;
}

void Timeline::clear()
{
   setGlobalStartTime( 0 );
   moveVerticalPositionPxl( 0.0f, Timeline::ANIMATION_TYPE_FAST );
   _bookmarks.times.clear();
   _rangeSelectTimeStamp[0] = _rangeSelectTimeStamp[1] = 0;
}

void Timeline::beginDrawCanvas( float canvasHeightPxl )
{
   ImGui::BeginChild(
       "TimelineCanvas",
       ImVec2( 0, 0 ),
       false,
       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
           ImGuiWindowFlags_NoMove );

   _canvasHeight = canvasHeightPxl;

   ImGui::SetScrollY( verticalPosPxl() );

   // Push clip rect for canvas and draw
   ImGui::PushClipRect( ImVec2( canvasPosX(), canvasPosY() ), ImVec2( 99999, 99999 ), true );
}

void Timeline::endDrawCanvas()
{
   ImGui::PopClipRect();
   ImGui::EndChild();
}

void Timeline::drawOverlay()
{
   char durationText[32] = {};
 
   const ImVec2 startDrawPos( _timelineDrawPosition[0], _timelineDrawPosition[1] );

   // Draw timeline mouse indicator
   if (_timelineHoverPos > 0.0f)
   {
      static char text[32] = {};
      const int64_t hoveredNano =
          _timelineStart +
          pxlToCycles( ImGui::GetWindowWidth(), _duration, _timelineHoverPos - startDrawPos.x );
      hop::formatCyclesTimepointToDisplay(
          hoveredNano, _duration, text, sizeof( text ), _displayType == DISPLAY_CYCLES );
      drawHoveringTimelineLine( _timelineHoverPos, startDrawPos.y, text );
   }

   // Draw bookmarks
   const ImVec2 windowSize = ImGui::GetWindowSize();
   if( !_bookmarks.times.empty() )
   {
      ImGui::PushClipRect(
          startDrawPos,
          ImVec2( startDrawPos.x + windowSize.x, startDrawPos.y + windowSize.y ),
          false );
      ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.8f, 1.0f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.0f, 0.0f, 0.9f, 1.0f ));
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.0f, 0.0f, 1.0f, 1.0f ));
      for( auto t : _bookmarks.times )
      {
         float posXPxl = cyclesToPxl( windowSize.x, _duration, t - _timelineStart );
         drawBookmarks( posXPxl + startDrawPos.x, startDrawPos.y );
      }
      ImGui::PopClipRect();
      ImGui::PopStyleColor(3);
   }


   // Draw zoom region
   if( _rangeZoomCycles[0] != _rangeZoomCycles[1] )
   {
      const auto minmaxCycles = std::minmax( _rangeZoomCycles[0], _rangeZoomCycles[1] );
      const float startPxl =
          cyclesToPxl<float>( windowSize.x, _duration, minmaxCycles.first - _timelineStart );
      const TimeDuration deltaCycles = minmaxCycles.second - minmaxCycles.first;
      const float durationPxl = cyclesToPxl( windowSize.x, _duration, deltaCycles );

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      drawList->AddRectFilled(
          ImVec2( startPxl, 0 ), ImVec2( startPxl + durationPxl, 9999 ), ImColor( 255, 255, 255, 64 ) );
   }

   // Draw selection region
   if( _rangeSelectTimeStamp[0] != _rangeSelectTimeStamp[1] )
   {
      const auto minmaxCycles = std::minmax( _rangeSelectTimeStamp[0], _rangeSelectTimeStamp[1] );
      const float startPxl =
          cyclesToPxl<float>( windowSize.x, _duration, minmaxCycles.first - _timelineStart );
      const TimeDuration deltaCycles = minmaxCycles.second - minmaxCycles.first;
      const float durationPxl = cyclesToPxl( windowSize.x, _duration, deltaCycles );

      hop::formatCyclesDurationToDisplay(
          deltaCycles,
          durationText,
          sizeof( durationText ),
          _displayType == DISPLAY_CYCLES );
      drawRangeSelection( startPxl, startPxl + durationPxl, _canvasDrawPosition[1], durationText );
   }
}

TimelineInfo Timeline::constructTimelineInfo() const noexcept
{
   return TimelineInfo{canvasPosX(),
                       canvasPosYWithScroll(),
                       verticalPosPxl(),
                       globalStartTime(),
                       relativeStartTime(),
                       duration(),
                       _rangeZoomCycles[0] != 0,
                       _displayType == DISPLAY_CYCLES};
}

void Timeline::drawTimeline( float posX, float posY )
{
   HOP_PROF_FUNC();

   constexpr uint64_t minStepSize = 10;
   constexpr uint64_t minStepCount = 20;
   constexpr uint64_t maxStepCount = 200;

   const float windowWidthPxl = ImGui::GetWindowWidth();

   ImGui::BeginChild("Timeline", ImVec2( windowWidthPxl, TIMELINE_TOTAL_HEIGHT) );

   uint64_t stepsCount = _duration / _stepSize;
   if( stepsCount > maxStepCount )
   {
      while( stepsCount > maxStepCount )
      {
         _stepSize *= 10;
         stepsCount = _duration / _stepSize;
      }
   }
   else if( stepsCount < minStepCount )
   {
      while( stepsCount < minStepCount && _stepSize > minStepSize )
      {
         _stepSize = std::max( _stepSize / 10, minStepSize );
         stepsCount = _duration / _stepSize;
      }
   }

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

   const float stepSizePxl = cyclesToPxl<float>( windowWidthPxl, _duration, _stepSize );
   const auto stepsDone = lldiv( _timelineStart, _stepSize );

   const float remainderPxl =
       cyclesToPxl<float>( windowWidthPxl, _duration, std::abs( stepsDone.rem ) );

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top( posX, posY );
   top.x -= hop::sign( stepsDone.rem ) * remainderPxl;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int64_t count = stepsDone.quot;
   TimelineTextPositions textPos;
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
             ImVec2( startEndLine.x, startEndLine.y + 5.0f ), count * _stepSize );
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

   // Draw the labels
   switch( _displayType )
   {
      case DISPLAY_CYCLES:
         drawTextPositionsCycles( textPos );
         break;
      case DISPLAY_TIMES:
         drawTextPositionsTime( textPos, _duration );
         break;
   }

   ImGui::EndChild();

   if (ImGui::IsItemHovered())
   {
      const auto curMousePosInScreen = ImGui::GetMousePos();
      _timelineHoverPos = curMousePosInScreen.x;
      if( ImGui::IsMouseClicked( 1 ) )
      {
         _bookmarks.times.push_back( _timelineStart + pxlToCycles(ImGui::GetWindowWidth(), _duration, _timelineHoverPos - posX) );
         std::sort( _bookmarks.times.begin(), _bookmarks.times.end() );
      }
   }
   else
   {
      _timelineHoverPos = -1.0f;
   }

   ImGui::SetCursorScreenPos( ImVec2{posX, posY + TIMELINE_TOTAL_HEIGHT } );
}

bool Timeline::handleMouse( float posX, float posY, bool /*lmPressed*/, bool /*rmPressed*/, float wheel )
{
   bool handled = false;
   if ( ImGui::IsItemHoveredRect() )
   {
      const ImVec2 mousePosInCanvas =
          ImVec2( posX - _timelineDrawPosition[0], posY - _timelineDrawPosition[1] );

      if ( ImGui::IsRootWindowOrAnyChildHovered() )
      {
         handleMouseWheel( mousePosInCanvas.x, wheel );
         handled = true;
      }

      if ( ImGui::IsRootWindowOrAnyChildFocused() )
      {
         handleMouseDrag( mousePosInCanvas.x, mousePosInCanvas.y );
         handled = true;

         // Handle left mouse click to reset range selection
         if( ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked( 0 ) )
            _rangeSelectTimeStamp[0] = _rangeSelectTimeStamp[1] = 0;
      }
   }

   return handled;
}

bool Timeline::handleHotkey()
{
   return true;
}

void Timeline::handleMouseWheel( float mousePosX, float mouseWheel )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // Handle vertical scroll
   if( ImGui::IsKeyDown( SDL_SCANCODE_LSHIFT ) )
   {
      constexpr float scrollAmount = 50.0f;
      if( mouseWheel > 0)
      {
         moveVerticalPositionPxl( verticalPosPxl() - scrollAmount, ANIMATION_TYPE_NONE );
      }
      else if( mouseWheel < 0 )
      {
         moveVerticalPositionPxl( verticalPosPxl() + scrollAmount, ANIMATION_TYPE_NONE );
      }
   }
   else // Handle zoom
   {
      if( mouseWheel > 0)
      {
         zoomOn( pxlToCycles( windowWidthPxl, _duration, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 0.5 : 0.9f );
      }
      else if( mouseWheel < 0 )
      {
         zoomOn( pxlToCycles( windowWidthPxl, _duration, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) ? 1.5 : 1.1f );
      }
   }
}

void Timeline::handleMouseDrag( float mouseInCanvasX, float /*mouseInCanvasY*/ )
{   
   const float windowWidthPxl = ImGui::GetWindowWidth();

   const int64_t mousePosAsCycles =
          _timelineStart + pxlToCycles<int64_t>( windowWidthPxl, _duration, mouseInCanvasX );

   // Ctrl + left mouse dragging ( Range Selection )
   if( ImGui::GetIO().KeyCtrl && ImGui::IsMouseDragging( 0 ) )
   {
      // If it is the first time we enter, setup the start position
      if ( _rangeSelectTimeStamp[0] == 0 )
      {
         _rangeSelectTimeStamp[0] = mousePosAsCycles;
         setRealtime( false );
      }
      // Continuously update the range
      _rangeSelectTimeStamp[1] = mousePosAsCycles;
   }
   // Left mouse button dragging ( Panning )
   else if ( ImGui::IsMouseDragging( 0 ) )
   {
      const auto delta = ImGui::GetMouseDragDelta();

      // Set horizontal position
      const int64_t deltaXInCycles =
          pxlToCycles<int64_t>( windowWidthPxl, _duration, delta.x );
      setStartTime( _timelineStart - deltaXInCycles, ANIMATION_TYPE_NONE );

      moveVerticalPositionPxl( _verticalPosPxl - delta.y, ANIMATION_TYPE_NONE );

      ImGui::ResetMouseDragDelta();
      setRealtime( false );
   }
   // Right mouse button dragging ( Range Zoom )
   else if ( ImGui::IsMouseDragging( 1 ) )
   {
      // If it is the first time we enter
      if ( _rangeZoomCycles[0] == 0 )
      {
         _rangeZoomCycles[0] = mousePosAsCycles;
         setRealtime( false );
      }
      _rangeZoomCycles[1] = mousePosAsCycles;
   }

   // Handle right mouse click up. (Finished right click selection zoom)
   if ( ImGui::IsMouseReleased( 1 ) && _rangeZoomCycles[0] != 0 )
   {
       pushNavigationState();

       const auto minmaxPos = std::minmax( _rangeZoomCycles[0], _rangeZoomCycles[1] );
       const TimeDuration newZoomDuration = minmaxPos.second - minmaxPos.first;

       // Number of cycles that are clamped if we are past the minimum zoom possible
       const TimeDuration deltaClamped =
           newZoomDuration < MIN_CYCLES_TO_DISPLAY ? MIN_CYCLES_TO_DISPLAY - newZoomDuration : 0;
       setStartTime( minmaxPos.first - (deltaClamped/2) );
       setZoom( newZoomDuration );

       // Reset values
       _rangeZoomCycles[0] = _rangeZoomCycles[1] = 0;
   }
}

void Timeline::handleDeferredActions( const TimelineMsgArray& messages )
{
   for( unsigned i = 0; i < messages.size(); ++i )
   {
      const TimelineMessage& m = messages[i];
      switch( m.type )
      {
         case TimelineMessageType::FRAME_TO_TIME:
            frameToTime( m.frameToTime.time, m.frameToTime.duration, m.frameToTime.pushNavState );
            break;
         case TimelineMessageType::FRAME_TO_ABSOLUTE_TIME:
            frameToAbsoluteTime( m.frameToTime.time, m.frameToTime.duration, m.frameToTime.pushNavState );
            break;
         case TimelineMessageType::MOVE_VERTICAL_POS_PXL:
            moveVerticalPositionPxl( m.verticalPos.posPxl );
            break;
         case TimelineMessageType::MOVE_TO_PRESENT_TIME:
            moveToPresentTime( ANIMATION_TYPE_NONE );
            break;
      }
   }
}

void Timeline::setDisplayType( DisplayType type )
{
   _displayType = type;
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
   return _canvasHeight + canvasPosY();
}

float Timeline::canvasPosX() const noexcept
{
   return _canvasDrawPosition[0];
}

float Timeline::canvasPosY() const noexcept
{
   return _canvasDrawPosition[1];
}

float Timeline::canvasPosYWithScroll() const noexcept
{
   return _canvasDrawPosition[1] - _verticalPosPxl;
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
   const float clampedPos = hop::clamp( positionPxl, 0.0f, maxVerticalPosPxl() );
   _animationState.targetVerticalPosPxl = clampedPos;
   _animationState.type = animType;
   if (animType == ANIMATION_TYPE_NONE)
   {
      _verticalPosPxl = clampedPos;
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
   _animationState.targetTimelineRange = hop::clamp( timelineDuration, MIN_CYCLES_TO_DISPLAY, MAX_CYCLES_TO_DISPLAY );
   _animationState.type = animType;
   if( animType == ANIMATION_TYPE_NONE )
   {
      // We need to update it immediately as subsequent call might need it updated
      // before the next update
      _duration = _animationState.targetTimelineRange;
   }
}

void Timeline::zoomOn( int64_t cycleToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t cycleToZoom = cycleToZoomOn - _timelineStart;

   const auto prevTimelineRange = _duration;
   setZoom( _duration * zoomFactor, ANIMATION_TYPE_NONE );

   const int64_t prevPxlPos = cyclesToPxl( windowWidthPxl, prevTimelineRange, cycleToZoom );
   const int64_t newPxlPos = cyclesToPxl( windowWidthPxl, _duration, cycleToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToCycles( windowWidthPxl, _duration, pxlDiff );
      setStartTime( _timelineStart + timeDiff, ANIMATION_TYPE_NONE );
   }
}

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
