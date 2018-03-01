#include "Timeline.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "Lod.h"
#include "Stats.h"
#include "StringDb.h"
#include "TraceDetail.h"

#include "imgui/imgui.h"

static constexpr float TRACE_HEIGHT = 20.0f;
static constexpr float TRACE_VERTICAL_PADDING = 2.0f;
static constexpr uint64_t MIN_MICROS_TO_DISPLAY = 100;
static constexpr uint64_t MAX_MICROS_TO_DISPLAY = 900000000;
static constexpr float MIN_TRACE_LENGTH_PXL = 0.1f;

namespace hop
{

void Timeline::update( float deltaTimeMs ) noexcept
{
   if( _startMicros != _animationState.targetStartMicros )
   {
      int64_t delta = _animationState.targetStartMicros - _startMicros;
      if( std::abs( delta ) < 3 )
      {
         _startMicros = _animationState.targetStartMicros;
      }
      _startMicros += delta * deltaTimeMs * 0.01f;
   }

   if( _microsToDisplay != _animationState.targetMicrosToDisplay )
   {
      int64_t delta = _animationState.targetMicrosToDisplay - _microsToDisplay;
      if( std::abs( delta ) < 3 )
      {
         _microsToDisplay = _animationState.targetMicrosToDisplay;
      }
      _microsToDisplay += delta * deltaTimeMs * 0.01f;
   }
}

void Timeline::draw(
    const std::vector<ThreadInfo>& tracesPerThread,
    const std::vector<uint32_t>& threadIds,
    const StringDb& strDb )
{
   ImGui::BeginChild(
      "Traces",
      ImVec2(0, 0),
      false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

   const auto startDrawPos = ImGui::GetCursorScreenPos();
   drawTimeline( startDrawPos.x, startDrawPos.y + 5 );

   char threadName[128] = "Thread ";
   for ( size_t i = 0; i < tracesPerThread.size(); ++i )
   {
      snprintf(
          threadName + sizeof( "Thread" ), sizeof( threadName ), "%lu (id=%u)", i, threadIds[i] );
      const auto traceColor = ImColor::HSV( i / 7.0f, 0.6f, 0.6f );
      const auto threadHeaderColor = ImColor(
          traceColor.Value.x - 0.2f, traceColor.Value.y - 0.2f, traceColor.Value.z - 0.2f );
      ImGui::PushStyleColor( ImGuiCol_Button, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, threadHeaderColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, threadHeaderColor );
      ImGui::Button( threadName );
      ImGui::PopStyleColor( 3 );
      ImGui::Spacing();
      ImGui::Separator();

      const auto curPos = ImGui::GetCursorScreenPos();
      drawLockWaits( tracesPerThread, i, curPos.x, curPos.y );
      // Draw traces needs to be done after draw lock waits because it sets the position
      // for the next drawing. TODO : Fix that ... -_-
      drawTraces( tracesPerThread[i], i, curPos.x, curPos.y, strDb, traceColor);

      ImGui::InvisibleButton( "trace-padding", ImVec2( 20, 40 ) );
   }

   ImGui::EndChild();

   if ( ImGui::IsItemHoveredRect() && ImGui::IsRootWindowOrAnyChildFocused() )
   {
      ImVec2 mousePosInCanvas = ImVec2(
          ImGui::GetIO().MousePos.x - startDrawPos.x, ImGui::GetIO().MousePos.y - startDrawPos.y );
      handleMouseDrag( mousePosInCanvas.x, mousePosInCanvas.y );
      handleMouseWheel( mousePosInCanvas.x, mousePosInCanvas.y );
   }
}

void Timeline::drawTimeline( const float posX, const float posY )
{
   constexpr int64_t minStepSize = 10;
   constexpr int64_t minStepCount = 20;
   constexpr int64_t maxStepCount = 140;

   const float windowWidthPxl = ImGui::GetWindowWidth();

   const size_t stepsCount = [=]() {
      size_t stepsCount = _microsToDisplay / _stepSizeInMicros;
      while ( stepsCount > maxStepCount ||
              ( stepsCount < minStepCount && _stepSizeInMicros > minStepSize ) )
      {
         if ( stepsCount > maxStepCount )
         {
            if ( _stepSizeInMicros == minStepSize )
            {
               _stepSizeInMicros = 8000;
            }
            _stepSizeInMicros *= 5.0f;
         }
         else if ( stepsCount < minStepCount )
         {
            _stepSizeInMicros /= 5.0f;
            _stepSizeInMicros = std::max( _stepSizeInMicros, minStepSize );
         }
         stepsCount = _microsToDisplay / _stepSizeInMicros;
      }
      return stepsCount;
   }();

   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   // Start drawing the vertical lines on the timeline
   constexpr float smallLineLength = 10.0f;
   constexpr float deltaBigLineLength = 12.0f;  // The diff between the small line and big one
   constexpr float deltaMidLineLength = 7.0f;   // The diff between the small line and mid one

   const int64_t stepSizePxl = microsToPxl( windowWidthPxl, _microsToDisplay, _stepSizeInMicros );
   const int64_t stepsDone = _startMicros / _stepSizeInMicros;
   const int64_t remainder = _startMicros % _stepSizeInMicros;
   int remainderPxl = 0;
   if ( remainder != 0 ) remainderPxl = microsToPxl( windowWidthPxl, _microsToDisplay, remainder );

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top( posX, posY );
   top.x -= ( stepSizePxl + remainderPxl ) - stepSizePxl;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int count = stepsDone;
   std::vector<std::pair<ImVec2, double> > textPos;
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
             ImVec2( startEndLine.x, startEndLine.y + 5.0f ), count * _stepSizeInMicros );
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

   const int64_t total = stepsCount * _stepSizeInMicros;
   if ( total < 1000 )
   {
      // print as microsecs
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%d us", (int)pos.second );
      }
   }
   else if ( total < 1000000 )
   {
      // print as milliseconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f ms", (float)( pos.second ) / 1000.0f );
      }
   }
   else if ( total < 1000000000 )
   {
      // print as seconds
      for ( const auto& pos : textPos )
      {
         ImGui::SetCursorScreenPos( pos.first );
         ImGui::Text( "%.3f s", (float)( pos.second ) / 1000000.0f );
      }
   }

   ImGui::SetCursorScreenPos( ImVec2{posX, posY + 50.0f} );
}

void Timeline::handleMouseWheel( float mousePosX, float )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   ImGuiIO& io = ImGui::GetIO();
   if ( io.MouseWheel > 0 )
   {
      zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosX ) + _startMicros, 0.9f );
   }
   else if ( io.MouseWheel < 0 )
   {
      zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosX ) + _startMicros, 1.1f );
   }
}

void Timeline::handleMouseDrag( float mouseInCanvasX, float mouseInCanvasY )
{
   // Left mouse button dragging
   if ( ImGui::IsMouseDragging( 0 ) )
   {
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const auto delta = ImGui::GetMouseDragDelta();
      const int64_t deltaXInMicros =
          pxlToMicros<int64_t>( windowWidthPxl, _microsToDisplay, delta.x );
      setStartMicro( _startMicros - deltaXInMicros, false );

      // Switch to the traces context to modify the scroll
      ImGui::BeginChild( "Traces" );
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
      const int64_t minXinMicros =
        pxlToMicros<int64_t>( windowWidthPxl, _microsToDisplay, minX );
      setStartMicro( _startMicros + minXinMicros );
      setZoom( pxlToMicros<uint64_t>( windowWidthPxl, _microsToDisplay, maxX - minX ) );

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

int64_t Timeline::microsToDisplay() const noexcept { return _microsToDisplay; }

const TraceDetails& Timeline::getTraceDetails() const noexcept
{
   return _traceDetails;
}

void Timeline::clearTraceDetails()
{
   _traceDetails = TraceDetails{};
}

void Timeline::setStartMicro( int64_t timeInMicro, bool withAnimation /*= true*/ ) noexcept
{
   _animationState.targetStartMicros = timeInMicro;
   if( !withAnimation )
      _startMicros = timeInMicro;
}

void Timeline::moveToTime( int64_t timeInMicro, bool animate ) noexcept
{
   setStartMicro( timeInMicro - ( _microsToDisplay * 0.5 ), animate );
}

void Timeline::moveToStart( bool animate ) noexcept
{
   moveToTime( _microsToDisplay * 0.5f, animate );
   setRealtime( false );
}

void Timeline::moveToPresentTime( bool animate ) noexcept
{
   moveToTime( ( _absolutePresentTime - _absoluteStartTime ) / 1000, animate );
}

void Timeline::setZoom( uint64_t microsToDisplay, bool withAnimation /*= true*/ )
{
   _animationState.targetMicrosToDisplay = hop::clamp( microsToDisplay, MIN_MICROS_TO_DISPLAY, MAX_MICROS_TO_DISPLAY );
   if( !withAnimation )
      _microsToDisplay = _animationState.targetMicrosToDisplay;
}

void Timeline::zoomOn( int64_t microToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const size_t microToZoom = microToZoomOn - _startMicros;
   const auto prevMicrosToDisplay = _microsToDisplay;
   setZoom( _microsToDisplay * zoomFactor, false );

   const int64_t prevPxlPos = microsToPxl( windowWidthPxl, prevMicrosToDisplay, microToZoom );
   const int64_t newPxlPos = microsToPxl( windowWidthPxl, _microsToDisplay, microToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToMicros( windowWidthPxl, _microsToDisplay, pxlDiff );
      setStartMicro( _startMicros + timeDiff, false );
   }
}

void Timeline::drawTraces(
    const ThreadInfo& data,
    int threadIndex,
    const float posX,
    const float posY,
    const StringDb& strDb,
    const ImColor& color )
{
   if ( data.traces.starts.empty() ) return;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   struct DrawingInfo
   {
      ImVec2 posPxl;
      float lengthPxl;
      float deltaMs;
      size_t traceIndex;
   };

   static std::vector<DrawingInfo> tracesToDraw, lodTracesToDraw;
   DrawingInfo selTraceDrawingInfo{};
   tracesToDraw.clear();
   lodTracesToDraw.clear();

   // Find the best lodLevel for our current zoom
   const int lodLevel = [this]() {
      if ( _microsToDisplay < LOD_MICROS[0] / 2 ) return -1;

      int lodLevel = 0;
      while ( lodLevel < LOD_COUNT - 1 && _microsToDisplay > LOD_MICROS[lodLevel] )
      {
         ++lodLevel;
      }
      return lodLevel;
   }();

   g_stats.currentLOD = lodLevel;

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + ( _startMicros * 1000 );
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + ( _microsToDisplay * 1000 );

   TDepth_t maxDepth = 0;
   // If we do not use LOD, draw the traces normally.
   if ( lodLevel == -1 )
   {
      const auto it1 = std::lower_bound(
          data.traces.starts.begin(), data.traces.starts.end(), firstTraceAbsoluteTime );
      const auto it2 = std::upper_bound(
          data.traces.starts.begin(), data.traces.starts.end(), lastTraceAbsoluteTime );

      // The last trace of the current thread does not reach the current time
      if ( it1 == data.traces.starts.end() ) return;

      size_t firstTraceId = std::distance( data.traces.starts.begin(), it1 );
      size_t lastTraceId = std::distance( data.traces.starts.begin(), it2 );

      // We've located the first trace that starts after the first shown trace. We now
      // need to find the last trace at depth level 0 that could be shown, meaning its
      // endtime < firstTraceAbsoluteTime
      while ( firstTraceId > 0 && // Is a valid trace
              data.traces.depths[firstTraceId] != 0 && // Is a level 0 trace
              // Has its end time before the timeline start
              data.traces.starts[firstTraceId] + data.traces.deltas[firstTraceId] > firstTraceAbsoluteTime )
      {
         --firstTraceId;
      }

      // We have found the last trace that starts after the end time of the timeline. Just
      // take the next level 0 trace to make sure we display the complete stack of traces
      while ( lastTraceId < data.traces.depths.size() && data.traces.depths[lastTraceId] != 0 )
      {
         ++lastTraceId;
      }
      // We need to go one past the depth 0 so it is included in the rendering
      lastTraceId = std::min( lastTraceId + 1, data.traces.depths.size() );

      // Lets create the drawing info for the traces
      for ( size_t i = firstTraceId; i < lastTraceId; ++i )
      {
         const int64_t traceStartInMicros = ( data.traces.starts[i] - absoluteStart ) / 1000;
         const auto traceStartPxl = microsToPxl<float>(
             windowWidthPxl, _microsToDisplay, traceStartInMicros - _startMicros );
         const float traceLengthPxl =
             microsToPxl<float>( windowWidthPxl, _microsToDisplay, data.traces.deltas[i] / 1000 );
         const float traceTimeMs = data.traces.deltas[i] / 1000000.0f;

         // Skip trace if it is way smaller than treshold
         if ( traceLengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         const auto curDepth = data.traces.depths[i];
         maxDepth = std::max( curDepth, maxDepth );
         const auto tracePos = ImVec2(
             posX + traceStartPxl,
             posY + curDepth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) );

         tracesToDraw.push_back( DrawingInfo{tracePos, traceLengthPxl, traceTimeMs, i} );

         if( i == _selection.id )
         {
            selTraceDrawingInfo = DrawingInfo{tracePos, traceLengthPxl, traceTimeMs, i};
         }
      }
   }
   else
   {
      const auto& lods = data.traces._lods[lodLevel];
      LodInfo firstInfo = {firstTraceAbsoluteTime, 0, 0, 0, false};
      LodInfo lastInfo = {lastTraceAbsoluteTime, 0, 0, 0, false};
      auto it1 = std::lower_bound( lods.begin(), lods.end(), firstInfo );
      auto it2 = std::upper_bound( lods.begin(), lods.end(), lastInfo );

      // The last trace of the current thread does not reach the current time
      if ( it1 == lods.end() ) return;

      --it1; // We want the first trace that is < start time
      // Find the the first trace on the left and right that have a depth of 0. This prevents
      // traces that have a smaller depth than the one foune previously to vanish.
      while ( it1 != lods.begin() && (it1->start + it1->delta) > firstTraceAbsoluteTime )
      {
         --it1;
      }
      while( it1->depth != 0 ) --it1;

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
         const int64_t traceStartInMicros = ( t.start - absoluteStart ) / 1000;
         const auto traceStartPxl = microsToPxl<float>(
             windowWidthPxl, _microsToDisplay, traceStartInMicros - _startMicros );
         const float traceLengthPxl =
             microsToPxl<float>( windowWidthPxl, _microsToDisplay, t.delta / 1000 );
         const float traceTimeMs = t.delta / 1000000.0f;

         // Skip trace if it is way smaller than treshold
         if ( traceLengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         maxDepth = std::max( t.depth, maxDepth );
         const auto tracePos = ImVec2(
             posX + traceStartPxl,
             posY + t.depth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) );
         if ( t.isLoded )
            lodTracesToDraw.push_back(
                DrawingInfo{tracePos, traceLengthPxl, traceTimeMs, t.traceIndex} );
         else
            tracesToDraw.push_back(
                DrawingInfo{tracePos, traceLengthPxl, traceTimeMs, t.traceIndex} );

         if( i == _selection.lodIds[lodLevel] )
         {
            selTraceDrawingInfo = DrawingInfo{tracePos, traceLengthPxl, traceTimeMs, i};
         }
      }
   }

   _maxTraceDepthPerThread[threadIndex] =
       std::max( _maxTraceDepthPerThread[threadIndex], maxDepth );

   const bool leftMouseClicked = ImGui::IsMouseReleased( 0 );
   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( color.Value.x, color.Value.y, color.Value.z ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor(color.Value.x + 0.1f, color.Value.y + 0.1f, color.Value.z + 0.1f));
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor(color.Value.x + 0.2f, color.Value.y + 0.2f, color.Value.z + 0.2f));

   // Draw the loded traces
   char curName[512] = {};
   const char* menuAction = NULL;
   const char* const menuSaveAsJason = "Save as JSON";
   const char* const menuHelp = "Help";
   for ( const auto& t : lodTracesToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );

      ImGui::Button( "", ImVec2( t.lengthPxl, TRACE_HEIGHT ) );

      if ( ImGui::IsItemHovered() )
      {
         if ( t.lengthPxl > 3 )
         {
            ImGui::BeginTooltip();
            snprintf( curName, sizeof( curName ), "<Multiple Elements> (~%.3f ms)\n", t.deltaMs );
            ImGui::TextUnformatted( curName );
            ImGui::EndTooltip();
         }

         if ( leftMouseDblClicked )
         {
            const auto traceStartMicros =
                pxlToMicros( windowWidthPxl, _microsToDisplay, t.posPxl.x - posX );
            setStartMicro( _startMicros + traceStartMicros );
            setZoom( t.deltaMs * 1000 );
         }
         else if( leftMouseClicked )
         {
            // Find the non-loded trace that is the closest to the cursor and at the right depth
            const auto& mousePos = ImGui::GetMousePos();
            const TimeStamp mouseInAbsoluteTime =
               firstTraceAbsoluteTime + pxlToMicros( windowWidthPxl, _microsToDisplay, mousePos.x - posX ) * 1000;
            const auto it = std::lower_bound(
               data.traces.starts.begin(), data.traces.starts.end(), mouseInAbsoluteTime );
            const int depth = (t.posPxl.y - posY) / ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING );
            size_t traceIndex = std::distance( data.traces.starts.begin(), it );
            while( traceIndex < data.traces.depths.size() && data.traces.depths[ traceIndex ] != depth )
            {
               ++traceIndex;
            }
            selectTrace( data, threadIndex, traceIndex );
         }
         else if ( rightMouseClicked )
         {
            _traceDetails = createTraceDetails( data.traces, threadIndex, t.traceIndex );
         }
      }
   }

   ImGui::PopStyleColor( 3 );

   ImGui::PushStyleColor(ImGuiCol_Button, ImColor(color.Value.x, color.Value.y, color.Value.z));
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(color.Value.x + 0.1f, color.Value.y + 0.1f, color.Value.z + 0.1f));
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(color.Value.x + 0.2f, color.Value.y + 0.2f, color.Value.z + 0.2f));
   // Draw the non-loded traces
   for ( const auto& t : tracesToDraw )
   {
      const size_t traceIndex = t.traceIndex;
      strDb.formatTraceName(data.traces.classNameIds[traceIndex], data.traces.fctNameIds[traceIndex], curName, sizeof(curName));

      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( curName, ImVec2( t.lengthPxl, TRACE_HEIGHT ) );
      if ( ImGui::IsItemHovered() )
      {
         if ( t.lengthPxl > 3 )
         {
            size_t lastChar = strlen( curName );
            curName[lastChar] = ' ';
            ImGui::BeginTooltip();
            snprintf(
                curName + lastChar,
                sizeof( curName ) - lastChar,
                "(%.3f ms)\n   %s:%d ",
                t.deltaMs,
                strDb.getString( data.traces.fileNameIds[traceIndex] ),
                data.traces.lineNbs[traceIndex] );
            ImGui::TextUnformatted( curName );
            ImGui::EndTooltip();
         }

         if ( leftMouseDblClicked )
         {
            setZoom( t.deltaMs * 1000 );
            setStartMicro( ( data.traces.starts[traceIndex] - absoluteStart ) * 0.001 );
         }
         else if( leftMouseClicked )
         {
            selectTrace( data, threadIndex, traceIndex );
         }
         else if ( rightMouseClicked )
         {
            _traceDetails = createTraceDetails( data.traces, threadIndex, t.traceIndex );
         }
      }
   }
   ImGui::PopStyleColor( 3 );

   // Draw selected trace
   if( _selection.id != Timeline::Selection::NONE && _selection.threadIndex == threadIndex )
   {
      ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 1.0f, 1.0f, 1.0f, 0.5f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 1.0f, 1.0f, 1.0f, 0.4f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 1.0f, 1.0f, 0.4f ) );
      ImGui::SetCursorScreenPos( selTraceDrawingInfo.posPxl );
      ImGui::Button( "", ImVec2( selTraceDrawingInfo.lengthPxl, TRACE_HEIGHT ) );
      ImGui::PopStyleColor( 3 );
   }

   ImGui::SetCursorScreenPos( ImVec2{
       posX,
       posY + _maxTraceDepthPerThread[threadIndex] * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING )} );
}

void Timeline::highlightLockOwner(
    const std::vector<ThreadInfo>& infos,
    size_t threadIndex,
    const hop::LockWait& highlightedLockWait,
    const float posX,
    const float posY )
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

        const float startMicrosAsPxl =
            microsToPxl<float>( windowWidthPxl, _microsToDisplay, _startMicros );

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

               const int64_t lockTimeAsPxl = microsToPxl<float>(
                   windowWidthPxl,
                   _microsToDisplay,
                   ( ( lockWaitIt->end - absoluteStart ) / 1000 ) );
               const int64_t unlockTimeAsPxl = microsToPxl<float>(
                   windowWidthPxl, _microsToDisplay, ( ( last->time - absoluteStart ) / 1000 ) );

               DrawList->AddRectFilled(
                   ImVec2( posX - startMicrosAsPxl + lockTimeAsPxl, 0 ),
                   ImVec2(
                       posX - startMicrosAsPxl + unlockTimeAsPxl,
                       999999 ),
                   ImColor( 0, 255, 0, 64 ) );
            }

            --last;
        }
    }
}

void Timeline::drawLockWaits(
    const std::vector<ThreadInfo>& infos,
    size_t threadIndex,
    const float posX,
    const float posY )
{
    const auto& data = infos[threadIndex];
   if ( data._lockWaits.empty() ) return;

   const auto& lockWaits = data._lockWaits;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t startMicrosAsPxl =
       microsToPxl<int64_t>( windowWidthPxl, _microsToDisplay, _startMicros );

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + ( _startMicros * 1000 );
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + ( _microsToDisplay * 1000 );

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 0.8f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 0.9f, 0.0f, 0.0f ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 0.0f, 0.0f ) );
   for ( const auto& lw : lockWaits )
   {
      if ( lw.end >= firstTraceAbsoluteTime && lw.start <= lastTraceAbsoluteTime )
      {
         const int64_t startInMicros = ( ( lw.start - absoluteStart ) / 1000 );

         const auto startPxl =
             microsToPxl<float>( windowWidthPxl, _microsToDisplay, startInMicros );
         const float lengthPxl = microsToPxl<float>(
             windowWidthPxl, _microsToDisplay, ( lw.end - lw.start ) / 1000.0f );

         // Skip if it is way smaller than treshold
         if ( lengthPxl < MIN_TRACE_LENGTH_PXL ) continue;

         ImGui::SetCursorScreenPos( ImVec2(
             posX - startMicrosAsPxl + startPxl,
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
      const auto& lods = data.traces._lods[i];
      auto lodIt = std::lower_bound(
          lods.begin(), lods.end(), LodInfo{data.traces.starts[traceIndex], 0, 0, 0, false} );
      while( lodIt != lods.end() && lodIt->depth != wantedDepth )
      {
         ++lodIt;
      }
      _selection.lodIds[i] = std::distance( lods.begin(), lodIt );
   }

   setRealtime( false );

   g_stats.selectedTrace = traceIndex;
}

} // namespace hop