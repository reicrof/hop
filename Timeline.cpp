#include "Timeline.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "Lod.h"
#include "Stats.h"
#include "StringDb.h"
#include "TraceDetail.h"

#include "imgui/imgui.h"

#include <cmath>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static constexpr float TRACE_HEIGHT = 20.0f;
static constexpr float TRACE_VERTICAL_PADDING = 2.0f;
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

namespace hop
{

void Timeline::update( float deltaTimeMs ) noexcept
{
   if( _timelineStart != _animationState.targetTimelineStart )
   {
      int64_t delta = _animationState.targetTimelineStart - _timelineStart;
      if( std::abs( delta ) < 3 )
      {
         _timelineStart = _animationState.targetTimelineStart;
      }
      _timelineStart += delta * deltaTimeMs * 0.01f;
   }

   if( _timelineRange != _animationState.targetTimelineRange )
   {
      int64_t delta = _animationState.targetTimelineRange - _timelineRange;
      if( std::abs( delta ) < 3 )
      {
         _timelineRange = _animationState.targetTimelineRange;
      }
      _timelineRange += delta * deltaTimeMs * 0.01f;
   }

   static float x = 0.0f;
   x += 0.007f * deltaTimeMs;
   _animationState.highlightPercent = (std::sin( x ) + 1.3f) / 2.0f;
}

void Timeline::draw(
    const std::vector<ThreadInfo>& tracesPerThread,
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

   char threadName[128] = "Thread ";
   for ( size_t i = 0; i < tracesPerThread.size(); ++i )
   {
      snprintf(
          threadName + sizeof( "Thread" ), sizeof( threadName ), "%lu", i );
      const auto traceColor = ImColor::HSV( i / 7.0f, 0.6f, 0.6f );
      const auto threadHeaderColor = ImColor(
          traceColor.Value.x - 0.2f, traceColor.Value.y - 0.2f, traceColor.Value.z - 0.2f );
      ImGui::PushStyleColor( ImGuiCol_Button, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, threadHeaderColor );
      ImGui::Button( threadName );
      ImGui::PopStyleColor( 3 );
      ImGui::Separator();

      ImVec2 curDrawPos = ImGui::GetCursorScreenPos();
      drawTraces(tracesPerThread[i], i, curDrawPos.x, curDrawPos.y, strDb, traceColor);
      drawLockWaits( tracesPerThread, i, curDrawPos.x, curDrawPos.y );

      curDrawPos.y += tracesPerThread[i].traces.maxDepth * (TRACE_HEIGHT + TRACE_VERTICAL_PADDING) + 70;
      ImGui::SetCursorScreenPos(curDrawPos);
   }

   if (_timelineHoverPos > 0.0f)
   {
      static char text[32] = {};
      const int64_t hoveredNano = _timelineStart + pxlToNanos(ImGui::GetWindowWidth(), _timelineRange, _timelineHoverPos - startDrawPos.x);
      hop::formatNanosTimepointToDisplay(hoveredNano, _timelineRange, text, sizeof(text));
      drawHoveringTimelineLine(_timelineHoverPos, startDrawPos.y, text);
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
      zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, 0.9f );
   }
   else if ( io.MouseWheel < 0 )
   {
      zoomOn( pxlToNanos( windowWidthPxl, _timelineRange, mousePosX ) + _timelineStart, 1.1f );
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
      setStartTime( _timelineStart - deltaXInNanos, false );

      // Switch to the traces context to modify the scroll
      ImGui::BeginChild( "TimelineCanvas" );
      ImGui::SetScrollY( ImGui::GetScrollY() - delta.y );
      ImGui::EndChild();

      ImGui::ResetMouseDragDelta();
      setRealtime( false );
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
      const float minX = std::min( _rightClickStartPosInCanvas[0], mouseInCanvasX );
      const float maxX = std::max( _rightClickStartPosInCanvas[0], mouseInCanvasX );
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const int64_t minXinNanos =
        pxlToNanos<int64_t>( windowWidthPxl, _timelineRange, minX );
      setStartTime( _timelineStart + minXinNanos );
      setZoom( pxlToNanos<TimeDuration>( windowWidthPxl, _timelineRange, maxX - minX ) );

      // Reset position
      _rightClickStartPosInCanvas[0] = _rightClickStartPosInCanvas[1] = 0.0f;
   }
}

bool Timeline::realtime() const noexcept { return _realtime; }

void Timeline::setRealtime( bool isRealtime ) noexcept { _realtime = isRealtime; }

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

void Timeline::setStartTime( int64_t time, bool withAnimation /*= true*/ ) noexcept
{
   _animationState.targetTimelineStart = time;
   if( !withAnimation )
      _timelineStart = time;
}

void Timeline::moveToAbsoluteTime( TimeStamp time, bool animate ) noexcept
{
   moveToTime( time - _absoluteStartTime, animate );
}

void Timeline::moveToTime( int64_t time, bool animate ) noexcept
{
   setStartTime( time - ( _timelineRange * 0.5 ), animate );
}

void Timeline::moveToStart( bool animate ) noexcept
{
   moveToTime( _timelineRange * 0.5f, animate );
   setRealtime( false );
}

void Timeline::moveToPresentTime( bool animate ) noexcept
{
   moveToTime( ( _absolutePresentTime - _absoluteStartTime ), animate );
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

void Timeline::setZoom( TimeDuration timelineDuration, bool withAnimation /*= true*/ )
{
   _animationState.targetTimelineRange = hop::clamp( timelineDuration, MIN_NANOS_TO_DISPLAY, MAX_NANOS_TO_DISPLAY );
   if( !withAnimation )
      _timelineRange = _animationState.targetTimelineRange;
}

void Timeline::zoomOn( int64_t nanoToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t nanoToZoom = nanoToZoomOn - _timelineStart;
   const auto prevTimelineRange = _timelineRange;
   setZoom( _timelineRange * zoomFactor, false );

   const int64_t prevPxlPos = nanosToPxl( windowWidthPxl, prevTimelineRange, nanoToZoom );
   const int64_t newPxlPos = nanosToPxl( windowWidthPxl, _timelineRange, nanoToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToNanos( windowWidthPxl, _timelineRange, pxlDiff );
      setStartTime( _timelineStart + timeDiff, false );
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
   if ( data.traces.ends.empty() ) return;

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
   DrawingInfo selTraceDrawingInfo{};
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
          data.traces.ends.begin(), data.traces.ends.end(), firstTraceAbsoluteTime );
      const auto it2 = std::upper_bound(
          data.traces.ends.begin(), data.traces.ends.end(), lastTraceAbsoluteTime );

      // The last trace of the current thread does not reach the current time
      if ( it1 == data.traces.ends.end() ) return;

      size_t firstTraceId = std::distance( data.traces.ends.begin(), it1 );
      size_t lastTraceId = std::distance( data.traces.ends.begin(), it2 );

      // Find the the first trace on the left and right that have a depth of 0. This prevents
      // traces that have a smaller depth than the one foune previously to vanish.
      while ( firstTraceId > 0 && data.traces.depths[firstTraceId] != 0 )
      {
         --firstTraceId;
      }
      while ( lastTraceId < data.traces.depths.size() && data.traces.depths[lastTraceId] != 0 )
      {
         ++lastTraceId;
      }
      if ( lastTraceId < data.traces.depths.size() )
      {
         ++lastTraceId;
      }  // We need to go one past the depth 0

      for ( size_t i = firstTraceId; i < lastTraceId; ++i )
      {
         const TimeStamp traceEndTime = ( data.traces.ends[i] - absoluteStart );
         const auto traceEndPxl = nanosToPxl<float>(
             windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
         const float traceLengthPxl =
             nanosToPxl<float>( windowWidthPxl, _timelineRange, data.traces.deltas[i] );

         // Skip trace if it is way smaller than treshold
         if ( traceLengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         const auto curDepth = data.traces.depths[i];
         const auto tracePos = ImVec2(
             posX + traceEndPxl - traceLengthPxl,
             posY + curDepth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) );

         tracesToDraw.push_back( DrawingInfo{tracePos, data.traces.deltas[i], i, traceLengthPxl} );

         if( i == _selection.id )
         {
            selTraceDrawingInfo = DrawingInfo{tracePos, data.traces.deltas[i], i, traceLengthPxl};
         }

         for( const auto& tid : _highlightedTraces )
         {
            if( threadIndex == tid.second && i == tid.first )
            {
               highlightTraceToDraw.push_back( DrawingInfo{tracePos, data.traces.deltas[i], i, traceLengthPxl} );
            }
         }
      }
   }
   else
   {
      const auto& lods = data.traces.lods[lodLevel];
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
             posY + t.depth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) );
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

         if( i == _selection.lodIds[lodLevel] )
         {
            selTraceDrawingInfo = DrawingInfo{tracePos, t.delta, i, traceLengthPxl};
         }
      }
   }

   const bool leftMouseClicked = ImGui::IsMouseReleased( 0 );
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
         else if( leftMouseClicked )
         {
            // Find the non-loded trace that is the closest to the cursor and at the right depth
            const auto& mousePos = ImGui::GetMousePos();
            const TimeStamp mouseInAbsoluteTime =
               firstTraceAbsoluteTime + pxlToNanos( windowWidthPxl, _timelineRange, mousePos.x - posX );
            const auto it = std::lower_bound(
               data.traces.ends.begin(), data.traces.ends.end(), mouseInAbsoluteTime );
            const int depth = (t.posPxl.y - posY) / ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING );
            size_t traceIndex = std::distance( data.traces.ends.begin(), it );
            while( traceIndex < data.traces.depths.size() && data.traces.depths[ traceIndex ] != depth )
            {
               ++traceIndex;
            }
            selectTrace( data, threadIndex, traceIndex );
         }
         else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
         {
            _traceDetails = createTraceDetails( data.traces, threadIndex, t.traceIndex );
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
      snprintf( curName, sizeof(curName), "%s", strDb.getString( data.traces.fctNameIds[traceIndex] ) );

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
                strDb.getString( data.traces.fileNameIds[traceIndex] ),
                data.traces.lineNbs[traceIndex] );
            ImGui::TextUnformatted( curName );
            ImGui::EndTooltip();
         }

         if ( leftMouseDblClicked )
         {
            setZoom( t.duration );
            setStartTime(
                ( data.traces.ends[traceIndex] - data.traces.deltas[traceIndex] - absoluteStart ) );
         }
         else if( leftMouseClicked )
         {
            selectTrace( data, threadIndex, traceIndex );
         }
         else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
         {
            _traceDetails = createTraceDetails( data.traces, threadIndex, t.traceIndex );
         }
      }
   }
   ImGui::PopStyleColor( 3 );

   // Draw selected trace
   if( _selection.id != Timeline::Selection::NONE && _selection.threadIndex == threadIndex )
   {
      ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 1.0f, 1.0f, 1.0f, 0.5f * _animationState.highlightPercent ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * _animationState.highlightPercent ) );
      ImGui::SetCursorScreenPos( selTraceDrawingInfo.posPxl );
      ImGui::Button( "", ImVec2( selTraceDrawingInfo.lengthPxl, TRACE_HEIGHT ) );
      ImGui::PopStyleColor( 3 );
   }

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
    const float /*posY*/ )
{
    struct unlock_events_less_cmp
    {
       bool operator()( const hop::UnlockEvent& ue, TimeStamp startTime )
       {
          return ue.time < startTime;
       }

       bool operator()( TimeStamp startTime, const hop::UnlockEvent& ue )
       {
          return startTime < ue.time;
       }
    };

    struct lock_wait_less_cmp
    {
       bool operator()( const hop::LockWait& lw, TimeStamp startTime )
       {
          return lw.end < startTime;
       }

       bool operator()( TimeStamp startTime, const hop::LockWait& lw )
       {
          return startTime < lw.end;
       }
    };


    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const auto absoluteStart = _absoluteStartTime;
    for (size_t i = 0; i < infos.size(); ++i)
    {
        if (i == threadIndex) continue;

        auto last = std::upper_bound( infos[i]._unlockEvents.cbegin(), infos[i]._unlockEvents.cend(), highlightedLockWait.end, unlock_events_less_cmp() );

        // upper_bound returns the first that is bigger. Thus we need the one just before that
        --last;

        const float startNanosAsPxl =
            nanosToPxl<float>( windowWidthPxl, _timelineRange, _timelineStart );

        while( last != infos[i]._unlockEvents.cbegin() )
        {
            if( last->mutexAddress == highlightedLockWait.mutexAddress )
            {
               // We've gone to far, so early break
               if( last->time < highlightedLockWait.start )
                  break;

               // Find the associated lock wait
               auto lockWaitIt = std::lower_bound(
                   infos[i]._lockWaits.cbegin(),
                   infos[i]._lockWaits.cend(),
                   last->time,
                   lock_wait_less_cmp() );

               // lower_bound returns the first that does not compare smaller than the unlock time.
               // Therefore, we need to start from this iterator and find the first one that matches
               // the highlighted mutex
               --lockWaitIt;
               while ( lockWaitIt != infos[i]._lockWaits.cbegin() &&
                       lockWaitIt->mutexAddress != highlightedLockWait.mutexAddress )
               {
                  --lockWaitIt;
               }

               const int64_t lockTimeAsPxl = nanosToPxl<float>(
                   windowWidthPxl,
                   _timelineRange,
                   ( lockWaitIt->end - absoluteStart ) );
               const int64_t unlockTimeAsPxl = nanosToPxl<float>(
                   windowWidthPxl, _timelineRange, ( last->time - absoluteStart ) );

               DrawList->AddRectFilled(
                   ImVec2( posX - startNanosAsPxl + lockTimeAsPxl, 0 ),
                   ImVec2(
                       posX - startNanosAsPxl + unlockTimeAsPxl,
                       999999 ),
                   ImColor( 0, 255, 0, 64 ) );
            }

            --last;
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
   const int64_t startNanosAsPxl =
       nanosToPxl<int64_t>( windowWidthPxl, _timelineRange, _timelineStart );

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + _timelineStart;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + _timelineRange;

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 0.8f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 0.9f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 0.0f, 0.0f ) );
   for ( const auto& lw : lockWaits )
   {
      if ( lw.end >= firstTraceAbsoluteTime && lw.start <= lastTraceAbsoluteTime )
      {
         const int64_t startInNanos = ( lw.start - absoluteStart );

         const auto startPxl =
             nanosToPxl<float>( windowWidthPxl, _timelineRange, startInNanos );
         const float lengthPxl = nanosToPxl<float>(
             windowWidthPxl, _timelineRange, lw.end - lw.start );

         // Skip if it is way smaller than treshold
         if ( lengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         ImGui::SetCursorScreenPos( ImVec2(
             posX - startNanosAsPxl + startPxl,
             posY + lw.depth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) ) );
         ImGui::Button( "Lock", ImVec2( lengthPxl, 20.f ) );
         if (ImGui::IsItemHovered())
         {
             highlightLockOwner(infos, threadIndex, lw, posX, posY);
         }
      }
   }
   ImGui::PopStyleColor( 3 );
}

void Timeline::selectTrace( const ThreadInfo& data, uint32_t threadIndex, size_t traceIndex )
{
   _selection.threadIndex = threadIndex;
   _selection.id = traceIndex;
   int wantedDepth = data.traces.depths[ traceIndex ];
   for ( int i = 0; i < LOD_COUNT; ++i )
   {
      const auto& lods = data.traces.lods[i];
      auto lodIt = std::lower_bound(
          lods.begin(), lods.end(), LodInfo{data.traces.ends[traceIndex], 0, 0, 0, false} );
      while( lodIt != lods.end() && lodIt->depth != wantedDepth )
      {
         ++lodIt;
      }
      _selection.lodIds[i] = std::distance( lods.begin(), lodIt );
   }

   setRealtime( false );

   g_stats.selectedTrace = traceIndex;
}

void Timeline::addTraceToHighlight( const std::pair< size_t, uint32_t >& trace )
{
   _highlightedTraces.push_back( trace );
}

void Timeline::clearHighlightedTraces()
{
   _highlightedTraces.clear();
}

} // namespace hop