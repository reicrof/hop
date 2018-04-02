#include "Timeline.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "Lod.h"
#include "Stats.h"
#include "StringDb.h"
#include "TraceDetail.h"

#include "imgui/imgui.h"

#include <SDL_keycode.h>

#include <cmath>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static constexpr hop::TimeDuration MIN_NANOS_TO_DISPLAY = 500;
static constexpr hop::TimeDuration MAX_NANOS_TO_DISPLAY = 900000000000;
static constexpr float MIN_TRACE_LENGTH_PXL = 0.1f;

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

   struct lock_wait_less_cmp
   {
      bool operator()(const hop::LockWait& lw, hop::TimeStamp time)
      {
         return lw.end < time;
      }

      bool operator()(hop::TimeStamp time, const hop::LockWait& lw)
      {
         return time < lw.end;
      }
   };
}

namespace hop
{

void Timeline::update( float deltaTimeMs ) noexcept
{
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

   static float x = 0.0f;
   x += 0.007f * deltaTimeMs;
   _animationState.highlightPercent = (std::sin( x ) + 1.3f) / 2.0f;
}

void Timeline::draw(
    std::vector<ThreadInfo>& tracesPerThread,
    const StringDb& strDb )
{
   ImGui::BeginChild("TimelineAndCanvas");
   const auto startDrawPos = ImGui::GetCursorScreenPos();
   drawTimeline(startDrawPos.x, startDrawPos.y + 5);

   ImGui::BeginChild(
      "TimelineCanvas",
      ImVec2(0, 0),
      false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

   // Set the scroll and get it back from ImGui to have the clamped value
   ImGui::SetScrollY(_verticalPosPxl);

   char threadName[128] = "Thread ";
   for ( size_t i = 0; i < tracesPerThread.size(); ++i )
   {
      const bool threadHidden = tracesPerThread[i]._hidden;
      snprintf(
          threadName + sizeof( "Thread" ), sizeof( threadName ), "%lu", i );
      const auto traceColor = ImColor::HSV( i / 7.0f, 0.6f, 0.6f );
      auto threadHeaderColor = ImColor(
          traceColor.Value.x - 0.2f, traceColor.Value.y - 0.2f, traceColor.Value.z - 0.2f );
      if(threadHidden)
         threadHeaderColor = ImColor(0.4f, 0.4f, 0.4f);
      ImGui::PushStyleColor( ImGuiCol_Button, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, threadHeaderColor );
      if( ImGui::Button( threadName ) )
         tracesPerThread[i]._hidden = !threadHidden;
      ImGui::PopStyleColor( 3 );
      ImGui::Separator();

      tracesPerThread[i]._localTracesVerticalStartPos= ImGui::GetCursorPosY();
      tracesPerThread[i]._absoluteTracesVerticalStartPos = ImGui::GetCursorScreenPos().y;

      if (!threadHidden)
      {
         ImVec2 curDrawPos = ImGui::GetCursorScreenPos();
         drawTraces(tracesPerThread[i], i, curDrawPos.x, curDrawPos.y, strDb, traceColor);
         drawLockWaits(tracesPerThread, i, curDrawPos.x, curDrawPos.y);

         curDrawPos.y += tracesPerThread[i]._traces.maxDepth * PADDED_TRACE_SIZE + 70;
         ImGui::SetCursorScreenPos(curDrawPos);
      }
   }

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
      ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 0.0f, 0.0f, 0.8f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 0.0f, 0.0f, 0.9f ));
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 0.0f, 0.0f, 1.0f ));
      for( auto t : _bookmarks.times )
      {
         float posXPxl = nanosToPxl( windowSize.x, _timelineRange, t - _timelineStart );
         drawBookmarks( posXPxl + startDrawPos.x, startDrawPos.y );
      }
      ImGui::PopClipRect();
      ImGui::PopStyleColor(3);
   }

   ImGui::EndChild(); // TimelineCanvas

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
   ImGuiIO& io = ImGui::GetIO();
   if ( io.MouseWheel > 0 )
   {
      zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT) ? 0.5 : 0.9f );
   }
   else if ( io.MouseWheel < 0 )
   {
      zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT) ? 1.5 : 1.1f );
   }
}

void Timeline::handleMouseDrag( float mouseInCanvasX, float mouseInCanvasY )
{
   // Left mouse button dragging
   if ( ImGui::IsMouseDragging( 0 ) )
   {
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const auto delta = ImGui::GetMouseDragDelta();
      const int64_t deltaXInNanos =
          pxlToNanos<int64_t>( windowWidthPxl, _timelineRange, delta.x );
      setStartTime( _timelineStart - deltaXInNanos, ANIMATION_TYPE_NONE );
   
      // Switch to the traces context to get scroll info
      ImGui::BeginChild("TimelineCanvas");
      const float maxScrollY = ImGui::GetScrollMaxY();
      ImGui::EndChild();

      _verticalPosPxl = hop::clamp(_verticalPosPxl - delta.y, 0.0f, maxScrollY);

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

float Timeline::verticalPosPxl() const noexcept
{
   return _verticalPosPxl;
}

const TraceDetails& Timeline::getTraceDetails() const noexcept
{
   return _traceDetails;
}

void Timeline::clearTraceDetails()
{
   _traceDetails = TraceDetails{};
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

void Timeline::drawTraces(
    const ThreadInfo& data,
    uint32_t threadIndex,
    const float posX,
    const float posY,
    const StringDb& strDb,
    const ImColor& color )
{
   if ( data._traces.ends.empty() ) return;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   struct DrawingInfo
   {
      ImVec2 posPxl;
      TimeDuration duration;
      size_t traceIndex;
      float lengthPxl;
   };

   static std::vector<DrawingInfo> tracesToDraw, lodTracesToDraw, highlightTraceToDraw;
   tracesToDraw.clear();
   lodTracesToDraw.clear();
   highlightTraceToDraw.clear();

   // Find the best lodLevel for our current zoom
   const int lodLevel = [this]() {
      if ( _timelineRange < LOD_NANOS[0] / 2 ) return -1;

      int lodLevel = 0;
      while ( lodLevel < LOD_COUNT - 1 && _timelineRange > LOD_NANOS[lodLevel] )
      {
         ++lodLevel;
      }
      return lodLevel;
   }();

   g_stats.currentLOD = lodLevel;

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + _timelineStart;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + _timelineRange;

   // If we do not use LOD, draw the traces normally.
   if ( lodLevel == -1 )
   {
      const auto it1 = std::lower_bound(
          data._traces.ends.begin(), data._traces.ends.end(), firstTraceAbsoluteTime );
      const auto it2 = std::upper_bound(
          data._traces.ends.begin(), data._traces.ends.end(), lastTraceAbsoluteTime );

      // The last trace of the current thread does not reach the current time
      if ( it1 == data._traces.ends.end() ) return;

      size_t firstTraceId = std::distance( data._traces.ends.begin(), it1 );
      size_t lastTraceId = std::distance( data._traces.ends.begin(), it2 );

      // Find the the first trace on the left and right that have a depth of 0. This prevents
      // traces that have a smaller depth than the one foune previously to vanish.
      while ( firstTraceId > 0 && data._traces.depths[firstTraceId] != 0 )
      {
         --firstTraceId;
      }
      while ( lastTraceId < data._traces.depths.size() && data._traces.depths[lastTraceId] != 0 )
      {
         ++lastTraceId;
      }
      if ( lastTraceId < data._traces.depths.size() )
      {
         ++lastTraceId;
      }  // We need to go one past the depth 0

      for ( size_t i = firstTraceId; i < lastTraceId; ++i )
      {
         const TimeStamp traceEndTime = ( data._traces.ends[i] - absoluteStart );
         const auto traceEndPxl = nanosToPxl<float>(
             windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
         const float traceLengthPxl =
             nanosToPxl<float>( windowWidthPxl, _timelineRange, data._traces.deltas[i] );

         // Skip trace if it is way smaller than treshold
         if ( traceLengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         const auto curDepth = data._traces.depths[i];
         const auto tracePos = ImVec2(
             posX + traceEndPxl - traceLengthPxl,
             posY + curDepth * PADDED_TRACE_SIZE);

         tracesToDraw.push_back( DrawingInfo{tracePos, data._traces.deltas[i], i, traceLengthPxl} );

         for( const auto& tid : _highlightedTraces )
         {
            if( threadIndex == tid.second && i == tid.first )
            {
               highlightTraceToDraw.push_back( DrawingInfo{tracePos, data._traces.deltas[i], i, traceLengthPxl} );
            }
         }
      }
   }
   else
   {
      const auto& lods = data._traces.lods[lodLevel];
      LodInfo firstInfo = {firstTraceAbsoluteTime, 0, 0, 0, false};
      LodInfo lastInfo = {lastTraceAbsoluteTime, 0, 0, 0, false};
      auto it1 = std::lower_bound( lods.begin(), lods.end(), firstInfo );
      auto it2 = std::upper_bound( lods.begin(), lods.end(), lastInfo );

      // The last trace of the current thread does not reach the current time
      if ( it1 == lods.end() ) return;

      // Find the the first trace on the left and right that have a depth of 0. This prevents
      // traces that have a smaller depth than the one foune previously to vanish.
      while ( it1 != lods.begin() && it1->depth != 0 )
      {
         --it1;
      }
      while ( it2 != lods.end() && it2->depth != 0 )
      {
         ++it2;
      }
      if ( it2 != lods.end() )
      {
         ++it2;
      }  // We need to go one past the depth 0

      const size_t firstTraceId = std::distance( lods.begin(), it1 );
      const size_t lastTraceId = std::distance( lods.begin(), it2 );

      for ( size_t i = firstTraceId; i < lastTraceId; ++i )
      {
         const auto& t = lods[i];
         const TimeStamp traceEndTime = ( t.end - absoluteStart );
         const auto traceEndPxl = nanosToPxl<float>(
             windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
         const float traceLengthPxl =
             nanosToPxl<float>( windowWidthPxl, _timelineRange, t.delta );

         // Skip trace if it is way smaller than treshold
         if ( traceLengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         const auto tracePos = ImVec2(
             posX + traceEndPxl - traceLengthPxl,
             posY + t.depth * PADDED_TRACE_SIZE);
         if ( t.isLoded )
         {
            lodTracesToDraw.push_back(
                DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
         }
         else
         {
            tracesToDraw.push_back(
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
   }

   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( color.Value.x, color.Value.y, color.Value.z ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor(color.Value.x + 0.1f, color.Value.y + 0.1f, color.Value.z + 0.1f));
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor(color.Value.x + 0.2f, color.Value.y + 0.2f, color.Value.z + 0.2f));

   // Draw the loded traces
   char curName[512] = "<Multiple Elements> ~";
   const size_t hoveredNamePrefixSize = strlen( curName );
   for ( const auto& t : lodTracesToDraw )
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
            const TimeStamp traceEndTime =
                pxlToNanos( windowWidthPxl, _timelineRange, t.posPxl.x - posX + t.lengthPxl );
            frameToTime( _timelineStart + ( traceEndTime - t.duration ), t.duration );
         }
         else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
         {
            _traceDetails = createTraceDetails( data._traces, threadIndex, t.traceIndex );
         }
      }
   }

   ImGui::PopStyleColor( 3 );

   ImGui::PushStyleColor(ImGuiCol_Button, ImColor(color.Value.x, color.Value.y, color.Value.z));
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(color.Value.x + 0.1f, color.Value.y + 0.1f, color.Value.z + 0.1f));
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(color.Value.x + 0.2f, color.Value.y + 0.2f, color.Value.z + 0.2f));
   char formattedTime[64] = {};
   // Draw the non-loded traces
   for ( const auto& t : tracesToDraw )
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
         else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
         {
            _traceDetails = createTraceDetails( data._traces, threadIndex, t.traceIndex );
         }
      }
   }
   ImGui::PopStyleColor( 3 );

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 1.0f, 1.0f, 1.0f, 0.5f * _animationState.highlightPercent ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ) );
   for( const auto& t : highlightTraceToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
   }
   ImGui::PopStyleColor( 3 );
}

void Timeline::highlightLockOwner(
    const std::vector<ThreadInfo>& infos,
    uint32_t threadIndex,
    const hop::LockWait& highlightedLockWait,
    const float posX,
    const float posY )
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const auto absoluteStart = _absoluteStartTime;
    for (size_t i = 0; i < infos.size(); ++i)
    {
        if (i == threadIndex) continue;

        const float startNanosAsPxl =
           nanosToPxl<float>(windowWidthPxl, _timelineRange, _timelineStart);

        auto lastUnlock = std::lower_bound( infos[i]._unlockEvents.cbegin(), infos[i]._unlockEvents.cend(), highlightedLockWait.end, unlock_events_less_cmp() );

        // lower_bound returns the first that is not smaller. We need the one just before that
        if(lastUnlock != infos[i]._unlockEvents.cbegin() ) --lastUnlock;

        const int highlightAlpha = 70.0f * _animationState.highlightPercent;

        while(lastUnlock != infos[i]._unlockEvents.cbegin() )
        {
            if(lastUnlock->mutexAddress == highlightedLockWait.mutexAddress )
            {
               // We've gone to far, so early break
               if(lastUnlock->time < highlightedLockWait.start )
                  break;

               // Find the associated lock wait
               auto lockWaitIt = std::lower_bound(
                   infos[i]._lockWaits.cbegin(),
                   infos[i]._lockWaits.cend(),
                   lastUnlock->time,
                   lock_wait_less_cmp() );

               // lower_bound returns the first that does not compare smaller than the unlock time.
               // Therefore, we need to start from this iterator and find the first one that matches
               // the highlighted mutex
               if( lockWaitIt != infos[i]._lockWaits.cbegin() ) --lockWaitIt;

               while ( lockWaitIt != infos[i]._lockWaits.cbegin() &&
                       lockWaitIt->mutexAddress != highlightedLockWait.mutexAddress )
               {
                  --lockWaitIt;
               }

               const int64_t lockTimeAsPxl = nanosToPxl<float>(
                  windowWidthPxl,
                  _timelineRange,
                  (lockWaitIt->end - absoluteStart));
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
}

void Timeline::drawLockWaits(
    const std::vector<ThreadInfo>& infos,
    uint32_t threadIndex,
    const float posX,
    const float posY )
{
   const auto& data = infos[threadIndex];
   if ( data._lockWaits.empty() ) return;

   const auto& lockWaits = data._lockWaits;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + _timelineStart;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + _timelineRange;

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 0.8f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 0.9f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 0.0f, 0.0f ) );

   const auto firstLwToDraw = std::lower_bound(
      lockWaits.cbegin(),
      lockWaits.cend(),
      firstTraceAbsoluteTime,
      lock_wait_less_cmp());

   // We need to find the first trace that starts after the end of the timeline
   // Since the LW are sorted according to the end time, we need to do a linear
   // search from the first trace that ends at the timeline end time
   auto lastLwToDraw = std::lower_bound(
      lockWaits.cbegin(),
      lockWaits.cend(),
      lastTraceAbsoluteTime,
      lock_wait_less_cmp());
   while( lastLwToDraw != lockWaits.end() && lastLwToDraw->start < lastTraceAbsoluteTime )
      ++lastLwToDraw;

   for ( auto it = firstLwToDraw; it != lockWaits.end(); ++it )
   {
      const int64_t startInNanos = ( it->start - absoluteStart - _timelineStart );

      const auto startPxl =
            nanosToPxl<float>( windowWidthPxl, _timelineRange, startInNanos );
      const float lengthPxl = nanosToPxl<float>(
            windowWidthPxl, _timelineRange, it->end - it->start );

      // Skip if it is way smaller than treshold
      if ( lengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

      ImGui::SetCursorScreenPos( ImVec2(
            posX + startPxl,
            posY + it->depth * PADDED_TRACE_SIZE) );
      ImGui::Button( "Acquiring Lock...", ImVec2( lengthPxl, Timeline::TRACE_HEIGHT ) );
      if (ImGui::IsItemHovered())
      {
            highlightLockOwner(infos, threadIndex, *it, posX, posY);
      }
   }
   ImGui::PopStyleColor( 3 );
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
