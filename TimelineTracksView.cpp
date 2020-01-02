#include "common/Profiler.h"
#include "common/TimelineTrack.h"
#include "common/TraceData.h"
#include "common/StringDb.h"
#include "common/Utils.h"

#include "Cursor.h"
#include "ModalWindow.h"
#include "SearchWindow.h"
#include "TimelineTracksView.h"
#include "TimelineInfo.h"
#include "Options.h"
#include "Stats.h"

#include "Lod.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT        = 20.0f;
static constexpr uint32_t DISABLED_COLOR          = 0xFF505050;
static constexpr uint32_t SEPARATOR_COLOR         = 0xFF666666;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR  = 0xFFAAAAAA;
static constexpr uint32_t CORE_LABEL_COLOR        = 0xFF333333;
static constexpr uint32_t CORE_LABEL_BORDER_COLOR = 0xFFAAAAAA;
static const char* CTXT_MENU_STR = "Context Menu";

// Static variable mutable from options
static float TRACE_VERTICAL_PADDING = 2.0f;
static float TRACE_HEIGHT = 20.0f;
static float PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

struct HighlightInfo
{
   hop::TimeStamp start, end;
   uint32_t threadIdx;
   hop::Depth_t depth;
};

struct LockOwnerInfo
{
   LockOwnerInfo( hop::TimeDuration dur, uint32_t tIdx ) : lockDuration( dur ), threadIndex( tIdx )
   {
   }
   hop::TimeDuration lockDuration{0};
   uint32_t threadIndex{0};
};

static void resetContextMenu( hop::TimelineTracksView::ContextMenu& ctxt )
{
   ctxt.traceId = 0;
   ctxt.threadIndex = 0;
   ctxt.traceClicked = false;
   ctxt.open = false;
}

static bool drawSeparator( uint32_t threadIndex, bool highlightSeparator )
{
   const float Y_PADDING   = 5.0f;
   const float windowWidth = ImGui::GetWindowWidth();
   const float handleWidth = 0.05f * windowWidth;
   const float drawPosY    = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y - Y_PADDING;
   ImVec2 p1               = ImGui::GetWindowPos();
   p1.y += drawPosY;

   ImVec2 p2( p1.x + windowWidth, p1.y + 1 );
   ImVec2 handleP2( p2.x - handleWidth, p2.y );

   const ImVec2 mousePos = ImGui::GetMousePos();
   const bool handledHovered =
       std::abs( mousePos.y - p1.y ) < 10.0f && mousePos.x > handleP2.x && threadIndex > 0;

   uint32_t color       = SEPARATOR_COLOR;
   uint32_t handleColor = SEPARATOR_HANDLE_COLOR;
   if( handledHovered && highlightSeparator )
   {
      hop::setCursor( hop::CURSOR_SIZE_NS );
      color = handleColor = 0xFFFFFFFF;
   }

   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddLine( p1, p2, color, 3.0f );
   drawList->AddLine( p2, handleP2, handleColor, 6.0f );

   ImGui::SetCursorPosY( ImGui::GetCursorPosY() );

   return handledHovered && highlightSeparator;
}

static void drawCoreLabels( const ImVec2& drawPosition, const hop::TimelineTrackDrawData& data, uint32_t threadIdx )
{
   using namespace hop;
   const auto& coreData = data.profiler.timelineTracks()[threadIdx]._coreEvents;
   if( options::showCoreInfo() && !coreData.data.empty() )
   {
      const auto drawStart = std::chrono::system_clock::now();

      const auto globalStartTime = data.timeline.globalStartTime;
      const float windowWidthPxl = ImGui::GetWindowWidth();

      // The time range to draw in absolute time
      const TimeStamp firstTraceAbsoluteTime = globalStartTime + data.timeline.relativeStartTime;
      const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + data.timeline.duration;

      CoreEvent firstEv = { firstTraceAbsoluteTime, firstTraceAbsoluteTime, 0 };
      CoreEvent lastEv = { lastTraceAbsoluteTime, lastTraceAbsoluteTime, 0 };
      auto cmp = []( const CoreEvent& lhs, const CoreEvent& rhs) { return lhs.end < rhs.end; };
      auto it1 = std::lower_bound( coreData.data.begin(), coreData.data.end(), firstEv, cmp );
      auto it2 = std::upper_bound( coreData.data.begin(), coreData.data.end(), lastEv, cmp );

      if( it2 != coreData.data.end() ) ++it2;

      if( it1 == it2 ) return; // Nothing to draw here


      ImGui::PushStyleColor( ImGuiCol_Button, CORE_LABEL_COLOR );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, CORE_LABEL_COLOR );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, CORE_LABEL_COLOR );
      ImGui::PushStyleColor( ImGuiCol_Border, CORE_LABEL_BORDER_COLOR );
      ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1.0f );

      char label[32] = "Core ";
      const int prefixSize = strlen( label );

      const float windowWidth = ImGui::GetWindowWidth();
      const float cyclesPerPxl = data.timeline.duration / windowWidth;

      const uint64_t minCycleToMerge = hop::pxlToCycles( windowWidthPxl, data.timeline.duration, 10 );
      const uint64_t minCycleToSkip = hop::pxlToCycles( windowWidthPxl, data.timeline.duration, 0.5f );
      CoreEvent prevEvent = *it1;

      // Lambda used to draw the labels inside the loop and for the last entry as well
      const auto drawCoreLabelFct = [&]( const CoreEvent& ev )
      {
         const float startPxl  = (int64_t)( ev.start - firstTraceAbsoluteTime ) / cyclesPerPxl;
         const std::pair<float,float> minMaxPxl = std::minmax( startPxl, 0.0f );

         auto deltaPxl = ( ev.end - ev.start ) / cyclesPerPxl;
         deltaPxl = std::min( deltaPxl + minMaxPxl.first, windowWidth );
         snprintf( label + prefixSize, sizeof( label ) - prefixSize, "%u", ev.core );

         ImGui::SetCursorScreenPos( ImVec2( minMaxPxl.second, drawPosition.y ) );
         ImGui::Button( label, ImVec2( deltaPxl, TRACE_HEIGHT - 1 ) );
         if( ImGui::IsItemHovered() )
         {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( label );
            ImGui::EndTooltip();
         }
      };

      for( ++it1 ; it1 != it2; ++it1 )
      {
         // If we have 2 consecutive traces with the same core and they are close enough, merge them
         // for drawing
         if( it1->core == prevEvent.core &&
            ( it1->start < prevEvent.end || ( it1->start - prevEvent.end ) < minCycleToMerge ) )
         {
            prevEvent.start = std::min( prevEvent.start, it1->start );
            prevEvent.end = it1->end;
            continue;
         }

         // Skip drawing the label if it is too small
         const auto labelDuration = prevEvent.end - prevEvent.start;
         if( labelDuration > minCycleToSkip )
         {
            drawCoreLabelFct( prevEvent );
         }

         prevEvent = *it1;
      }

      // Add the last entry to draw
      drawCoreLabelFct( prevEvent );

      ImGui::PopStyleVar(1);
      ImGui::PopStyleColor(4);

      const auto drawEnd = std::chrono::system_clock::now();
      hop::g_stats.coreDrawingTimeMs +=
         std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
   }
}

static bool drawThreadLabel(
    const ImVec2& drawPosition,
    const char* threadName,
    uint32_t trackIndex,
    bool hidden )
{
   char nameBuffer[128];
   if( threadName )
   {
      snprintf( nameBuffer, sizeof( nameBuffer ), "%s", threadName );
   }
   else
   {
      snprintf( nameBuffer, sizeof( nameBuffer ), "Thread %u", trackIndex );
   }

   const ImVec2 threadLabelSize = ImGui::CalcTextSize( nameBuffer );
   ImGui::PushClipRect(
       ImVec2( drawPosition.x + threadLabelSize.x + 8, drawPosition.y ),
       ImVec2( 99999.0f, 999999.0f ),
       true );

   const auto& zoneColors = hop::options::zoneColors();
   uint32_t threadLabelCol = zoneColors[( trackIndex + 1 ) % HOP_MAX_ZONE_COLORS];
   if( hidden )
   {
      threadLabelCol = DISABLED_COLOR;
   }

   ImGui::PopClipRect();

   // Draw thread label
   ImGui::SetCursorScreenPos( drawPosition );
   ImGui::PushID( trackIndex );
   ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
   const bool labelPressed = ImGui::Button( nameBuffer, ImVec2( 0, THREAD_LABEL_HEIGHT ) );
   ImGui::PopStyleColor();
   ImGui::PopID();

   return labelPressed;
}

static void drawTrackHighlight( float trackX, float trackY, float trackHeight )
{
   if( ImGui::IsRootWindowOrAnyChildFocused() )
   {
      const ImVec2 trackTopLeft = ImVec2( trackX, trackY );
      const ImVec2 trackBotRight = ImVec2( trackX + 9999, trackY + trackHeight );
      const ImVec2 mousePos = ImGui::GetMousePos();
      if ( hop::ptInRect(
               mousePos.x,
               mousePos.y,
               trackTopLeft.x,
               trackTopLeft.y,
               trackBotRight.x,
               trackBotRight.y ) )
      {
         ImDrawList* dl = ImGui::GetWindowDrawList();
         dl->AddRectFilled( trackTopLeft, trackBotRight, 0x03FFFFFF );
      }
   }
}

template <typename LodIt>
static void createDrawData(
    LodIt it,
    size_t count,
    hop::TimeStamp absStart,
    float cyclesPerPxl,
    float* __restrict startsPxl,
    float* __restrict deltaPxl )
{
   HOP_PROF_FUNC();
   for( size_t i = 0; i < count; ++i, ++it )
   {
      // Use the min max to clamp the starting position to 0 and remove what has been "cropped" from
      // the length
      const std::pair<float, float> minMaxPxl = // Do not use auto as minmax returns a reference
          std::minmax( ( int64_t )( it->start - absStart ) / cyclesPerPxl, 0.0f );
      startsPxl[i] = minMaxPxl.second;
      deltaPxl[i]  = std::max( ( ( it->end - it->start ) / cyclesPerPxl ) + minMaxPxl.first, 1.0f );
   }
}

static uint32_t setBitIndex( hop::ZoneId_t zone )
{
   uint32_t count = 0;
   while ( zone )
   {
      zone = zone >> 1;
      ++count;
   }
   return count-1;
}

/*
   Customization point for the draw entries function
*/
using GetEntryLabel = int (*)( const hop::TimelineTrackDrawData& data, uint32_t threadIdx, size_t entryIdx, hop::TimeDuration duration, uint32_t arrSize, char* arr );
static int traceLabelWithTime( const hop::TimelineTrackDrawData& data, uint32_t threadIdx, size_t entryIdx, hop::TimeDuration duration, uint32_t arrSize, char* arr )
{
   const auto& tracesData = data.profiler.timelineTracks()[threadIdx]._traces;
   const char* label = data.profiler.stringDb().getString( tracesData.fctNameIds[entryIdx] );

   char fmtTime[32];
   hop::formatCyclesDurationToDisplay( duration, fmtTime, sizeof(fmtTime), data.timeline.useCycles );

   return snprintf( arr, arrSize, "%s (%s)", label, fmtTime );
}
static int lockwaitLabelWithTime(const hop::TimelineTrackDrawData&, uint32_t, size_t, hop::TimeDuration, uint32_t arrSize, char* arr)
{
   return snprintf( arr, arrSize, "%s", "Lock wait" );
}

using GetEntryZone = hop::ZoneId_t (*)( const hop::TimelineTrackDrawData& data, uint32_t threadIdx, size_t entryIdx );
static hop::ZoneId_t getTraceZoneId( const hop::TimelineTrackDrawData& data, uint32_t threadIdx, size_t entryIdx )
{
   return data.profiler.timelineTracks()[threadIdx]._traces.zones[entryIdx];
}
static hop::ZoneId_t getLockWaitZoneId( const hop::TimelineTrackDrawData&, uint32_t, size_t )
{
   return 1;
}
/* ----------------------------------------------------- */

static void drawHoveredTracePopup( const hop::TimelineTrackDrawData& data, uint32_t threadIndex, size_t entryIndex )
{
   char strBuffer[512];
   const auto& tracesData = data.profiler.timelineTracks()[threadIndex]._traces;
   const hop::TimeDuration delta = tracesData.entries.ends[ entryIndex ] - tracesData.entries.starts[ entryIndex ];
   const int charWritten = traceLabelWithTime( data, threadIndex, entryIndex, delta, sizeof( strBuffer ), strBuffer );
   
   snprintf(
       strBuffer + charWritten,
       std::max( 0, (int)sizeof( strBuffer ) - charWritten ),
       "\n   %s:%d ",
       data.profiler.stringDb().getString( tracesData.fileNameIds[entryIndex] ),
       tracesData.lineNbs[entryIndex] );

   ImGui::TextUnformatted( strBuffer );

// Print out some debug info as well
#ifdef HOP_DEBUG
   char strDebug[512];
   const auto end = tracesData.entries.ends[entryIndex];
   snprintf(
       strDebug,
       sizeof( strDebug ),
       "\n======== Debug Info ========\n"
       "Trace Index = %zu\n"
       "Trace Start = %zu\n"
       "Trace End   = %zu\n"
       "Trace Delta = %zu",
       entryIndex,
       (size_t)(end - delta),
       (size_t)end,
       (size_t)delta );
   ImGui::TextUnformatted( strDebug );
#endif
}

static void drawHoveredLockWaitPopup(
    const void* mutexAddr,
    hop::TimeDuration duration,
    const std::vector<LockOwnerInfo>& locksInfo,
    bool drawAsCycles )
{
   char buffer[512];
   snprintf( buffer, sizeof( buffer ), "Waiting lock 0x%p for ", mutexAddr );
   int charWritten = strlen( buffer );
   charWritten += hop::formatCyclesDurationToDisplay(
       duration, buffer + charWritten, sizeof( buffer ) - charWritten, drawAsCycles );

   if( !locksInfo.empty() )
   {
      // Print infos about which threads own the lock
      char formattedLockTime[64] = {};
      for( const auto& i : locksInfo )
      {
         hop::formatCyclesDurationToDisplay(
             i.lockDuration, formattedLockTime, sizeof( formattedLockTime ), drawAsCycles );
         charWritten += snprintf(
             buffer + charWritten,
             sizeof( buffer ) - charWritten,
             "\n  Thread #%u (%s)",
             i.threadIndex,
             formattedLockTime );
      }
   }
   else
   {
      // Set a message to warn the user than the thread owning the lock is not part of
      // any profiled code
      snprintf(
          buffer + charWritten,
          sizeof( buffer ) - charWritten,
          "\n  Threads owning the lock were not profiled or the number of cycles required\n  to "
          "acquire the lock was smaller than what HOP_MIN_LOCK_CYCLES defines" );
   }
   ImGui::TextUnformatted( buffer );
}

static size_t drawEntries(
    const ImVec2 drawPos,
    uint32_t threadIndex,
    const hop::TimelineTrackDrawData& data,
    const hop::LodsData& lodsData,
    GetEntryLabel getEntryLabelFct,
    GetEntryZone getEntryZoneFct )
{
   using namespace hop;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   // Get all the timing boundaries
   const TimeStamp globalStartTime  = data.timeline.globalStartTime;
   const TimeStamp relativeStart    = data.timeline.relativeStartTime;
   const TimeDuration timelineRange = data.timeline.duration;

   const TimeStamp absoluteStart    = relativeStart + globalStartTime;
   const TimeStamp absoluteEnd      = absoluteStart + timelineRange;

   // The time range to draw in absolute time
   const auto spanIndex =
      hop::visibleIndexSpan( lodsData.lods, data.lodLevel, absoluteStart, absoluteEnd, 0 );

   if( spanIndex.first == hop::INVALID_IDX ) return hop::INVALID_IDX;

   static std::vector< float > startPosPxl;
   static std::vector< float > deltaPxl;

   const size_t traceCount = spanIndex.second - spanIndex.first;
   startPosPxl.resize( traceCount );
   deltaPxl.resize( traceCount );

   const float windowWidthPxl = ImGui::GetWindowWidth();

   const auto lodStartIt = lodsData.lods[data.lodLevel].begin() + spanIndex.first;
   createDrawData( lodStartIt, traceCount, absoluteStart, timelineRange / windowWidthPxl, startPosPxl.data(), deltaPxl.data() );

   const auto& zoneColors   = hop::options::zoneColors();
   const ImVec2 mousePos    = ImGui::GetMousePos();
   const ImVec2 textAlign   = ImVec2( 0.0f, 0.5f );
   const ImVec2 framePading = ImGui::GetStyle().FramePadding;

   char entryName[256] = {};
   size_t hoveredLodIdx = hop::INVALID_IDX;
   for( size_t i = 0; i < traceCount; ++i )
   {
      const LodInfo& curLod = *(lodStartIt + i);
      const size_t absIndex = curLod.index;

      const uint32_t zoneId = setBitIndex( getEntryZoneFct( data, threadIndex, absIndex ) );
      const ImVec2 from( startPosPxl[i], drawPos.y + curLod.depth * PADDED_TRACE_SIZE );
      const ImVec2 to( from + ImVec2( deltaPxl[i], PADDED_TRACE_SIZE ) );
      ImGui::RenderFrame( from, to, zoneColors[zoneId] );

      // Create the name for the trace if it is large enough on screen
      entryName[0] = '\0';
      if( deltaPxl[i] > 5.0f && !curLod.loded )
      {
         getEntryLabelFct( data, threadIndex, absIndex, curLod.end - curLod.start, sizeof(entryName), entryName );
         ImVec2 labelSize = ImGui::CalcTextSize(entryName, NULL, true);
         ImGui::RenderTextClipped( from + framePading, to - framePading, entryName, NULL, &labelSize, textAlign, nullptr );

         // Keep the index around if the mouse is inside the drawing
         if( hop::ptInRect( mousePos.x, mousePos.y, from.x, from.y, to.x, to.y ) )
         {
            hoveredLodIdx = i;
         }
      }
   }

   startPosPxl.clear();
   deltaPxl.clear();

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.traceDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();

   return hoveredLodIdx == hop::INVALID_IDX ? hop::INVALID_IDX : (lodStartIt + hoveredLodIdx)->index;
}

static void drawHighlightedTraces(
    const std::vector<hop::TimelineTracksView::TrackViewData>& tracksView,
    const hop::TimelineTrackDrawData& data,
    const std::vector<HighlightInfo>& traceToHighlight,
    float highlightVal )
{
   ImDrawList* drawList               = ImGui::GetWindowDrawList();
   const float wndWidth               = ImGui::GetWindowWidth();
   const uint32_t color               = 0x00FFFFFF | (uint32_t(highlightVal * 255.0f) << 24);
   const hop::TimeDuration tlDuration = data.timeline.duration;
   const hop::TimeDuration tlStart    = data.timeline.globalStartTime + data.timeline.relativeStartTime;
   for( const auto& trace : traceToHighlight )
   {
      const float startPxl = hop::cyclesToPxl<float>( wndWidth, tlDuration, trace.start - tlStart );
      const float endPxl = hop::cyclesToPxl<float>( wndWidth, tlDuration, trace.end - tlStart );
      const float posYPxl  =
         data.timeline.canvasPosY + tracksView[trace.threadIdx].absoluteDrawPos[1] + trace.depth * PADDED_TRACE_SIZE;

      drawList->AddRectFilled( ImVec2( startPxl, posYPxl ), ImVec2( endPxl, posYPxl + PADDED_TRACE_SIZE ), color );
   }
}

static void handleHoveredTrace(
   hop::TimelineTracksView::ContextMenu& contextMenu,
   const hop::TimelineTrackDrawData& data,
   unsigned threadIndex,
   size_t hoveredIdx,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   if( hoveredIdx != hop::INVALID_IDX )
   {
      ImGui::BeginTooltip();
      drawHoveredTracePopup( data, threadIndex, hoveredIdx );
      ImGui::EndTooltip();

      const auto& traceData = data.profiler.timelineTracks()[threadIndex]._traces;
      const hop::TimeStamp start = traceData.entries.starts[hoveredIdx];
      const hop::TimeStamp end   = traceData.entries.ends[hoveredIdx];
      const hop::Depth_t depth   = traceData.entries.depths[hoveredIdx];

      const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
      const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );
      // Handle mouse interaction
      if( leftMouseDblClicked )
      {
         msgArray->addFrameTimeMsg( start, end-start, true, true );
      }
      else if( rightMouseClicked && !data.timeline.mouseDragging )
      {
         ImGui::OpenPopup( CTXT_MENU_STR );
         contextMenu.open = true;
         contextMenu.traceClicked = true;
         contextMenu.threadIndex = threadIndex;
         contextMenu.traceId = hoveredIdx;
      }

      traceToHighlight.emplace_back(HighlightInfo{ start, end, threadIndex, depth });
   }
}

static std::vector<LockOwnerInfo> highlightLockOwner(
    const hop::TimelineTracksView& tracksView,
    const hop::TimelineTrackDrawData& data,
    const void* highlightedMutexAddr,
    uint32_t threadIdx,
    size_t hoveredLwIndex )
{
   using namespace hop;
   HOP_PROF_FUNC();
   std::vector<LockOwnerInfo> lockInfos;
   lockInfos.reserve( 16 );

   const std::vector<TimelineTrack>& tlt = data.profiler.timelineTracks();

   // Info of the currently highlighted lock wait
   const TimeStamp highlightedLWEnd   = tlt[threadIdx]._lockWaits.entries.ends[hoveredLwIndex];
   const TimeStamp highlightedLWStart = tlt[threadIdx]._lockWaits.entries.starts[hoveredLwIndex];

   // Data for drawing the owner's highlight
   ImDrawList* DrawList          = ImGui::GetOverlayDrawList();
   const float wndWidth          = ImGui::GetWindowWidth();
   const int highlightAlpha      = 70.0f * data.highlightValue;
   const TimeDuration tlDuration = data.timeline.duration;
   const TimeDuration tlStart    = data.timeline.globalStartTime + data.timeline.relativeStartTime;   

   // Go through all the threads and find the one that owned the lock during the
   // highlighted periods
   for( size_t i = 0; i < tlt.size(); ++i )
   {
      // Skip the current thread as it is obviously trying to acquire the lock...
      if( i == threadIdx ) continue;

      const LockWaitData& lockWaits = tlt[i]._lockWaits;

      const auto lastUnlock = std::lower_bound(
          lockWaits.entries.ends.cbegin(), lockWaits.entries.ends.cend(), highlightedLWEnd );

      // This is the first lockdata that was acquired after the highlighted trace end
      auto lockDataIdx = std::distance( lockWaits.entries.ends.cbegin(), lastUnlock );
      if( lockDataIdx != 0 ) --lockDataIdx;

      const float tracesHeight = tracksView.trackHeightWithThreadLabel( i );
      while( lockDataIdx != 0 )
      {
         if( lockWaits.mutexAddrs[lockDataIdx] == highlightedMutexAddr )
         {
            const TimeStamp lockWaitEndTime = lockWaits.entries.ends[lockDataIdx];
            const TimeStamp unlockTime      = lockWaits.lockReleases[lockDataIdx];

            // We've gone to far, so early break
            if( unlockTime != 0 && unlockTime < highlightedLWStart ) break;

            const TimeDuration lockHoldDuration = unlockTime - lockWaitEndTime;

            // Add info to result vector
            bool added = false;
            for( auto& info : lockInfos )
            {
               if( info.threadIndex == i )
               {
                  info.lockDuration += lockHoldDuration;
                  added = true;
                  break;
               }
            }
            if( !added ) lockInfos.emplace_back( lockHoldDuration, i );

            const float startPxl =
                hop::cyclesToPxl( wndWidth, tlDuration, lockWaitEndTime - tlStart );
            const float durationPxl = hop::cyclesToPxl( wndWidth, tlDuration, lockHoldDuration );
            const float posYPxl = data.timeline.canvasPosY + tracksView.trackAbsoluteDrawPosY( i );

            DrawList->AddRectFilled(
                ImVec2( startPxl, posYPxl ),
                ImVec2( startPxl + durationPxl, posYPxl + tracesHeight ),
                ImColor( 0, 255, 0, 30 + highlightAlpha ) );
         }

         --lockDataIdx;
      }
   }

   return lockInfos;
}

static void handleHoveredLockWait(
   const hop::TimelineTracksView& tracksView,
   const hop::TimelineTrackDrawData& data,
   unsigned threadIdx,
   size_t hoveredIdx,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   if( hoveredIdx != hop::INVALID_IDX )
   {
      const auto& lwData               = data.profiler.timelineTracks()[threadIdx]._lockWaits;
      const hop::TimeStamp start       = lwData.entries.starts[hoveredIdx];
      const hop::TimeStamp end         = lwData.entries.ends[hoveredIdx];
      const hop::Depth_t depth         = lwData.entries.depths[hoveredIdx];
      const void* highlightedMutexAddr = lwData.mutexAddrs[hoveredIdx];

      const std::vector<LockOwnerInfo> lockOwnerInfo =
          highlightLockOwner( tracksView, data, highlightedMutexAddr, threadIdx, hoveredIdx );

      ImGui::BeginTooltip();
      drawHoveredLockWaitPopup(
          highlightedMutexAddr, end - start, lockOwnerInfo, data.timeline.useCycles );
      ImGui::EndTooltip();

      // Handle mouse interaction
      if( ImGui::IsMouseDoubleClicked( 0 ) )
      {
         msgArray->addFrameTimeMsg( start, end-start, true, true );
      }

      traceToHighlight.emplace_back(HighlightInfo{ start, end, threadIdx, depth });
   }
}

static void handleHoveredTrack(
   hop::TimelineTracksView::ContextMenu& contextMenu,
   const std::vector<hop::TimelineTracksView::TrackViewData>& tracksView,
   const hop::TimelineTrackDrawData& data )
{
   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const float mousePosY = ImGui::GetMousePos().y;
   if( rightMouseClicked && !contextMenu.open && !data.timeline.mouseDragging &&
       mousePosY > data.timeline.canvasPosY )
   {
      ImGui::OpenPopup( CTXT_MENU_STR );
      contextMenu.open = true;

      // Find out where the right click happened to figure out which track needs to be profiled
      int trackIdx = tracksView.size() - 1;
      for( ; trackIdx >=0; --trackIdx)
      {
         if( tracksView[ trackIdx ].relativePosY - THREAD_LABEL_HEIGHT < mousePosY )
         {
            break;
         }
      }

      assert( trackIdx >= 0 );
      contextMenu.threadIndex = trackIdx;
   }
}

namespace hop
{

uint32_t TimelineTracksView::count() const
{
   return _tracks.size();
}

bool TimelineTracksView::hidden( uint32_t trackIdx ) const
{
   return _tracks[trackIdx].trackHeight <= -PADDED_TRACE_SIZE;
}

float TimelineTracksView::trackHeightWithThreadLabel( uint32_t trackIdx ) const
{
    return _tracks[trackIdx].trackHeight + THREAD_LABEL_HEIGHT;
}

float TimelineTracksView::trackAbsoluteDrawPosY( uint32_t trackIdx ) const
{
   return _tracks[trackIdx].absoluteDrawPos[1];
}

void TimelineTracksView::update( const hop::Profiler& profiler )
{
   HOP_PROF_FUNC();

   const auto updateStart = std::chrono::system_clock::now();

   {  // Add tracks that were added since last frame
      const std::vector<TimelineTrack>& tlTrackData = profiler.timelineTracks();
      const int newTrackCount = tlTrackData.size() - _tracks.size();
      assert( newTrackCount >= 0 );
      if( newTrackCount > 0 )
      {
         _tracks.insert( _tracks.end(), newTrackCount, TrackViewData{} );
      }
   }

   // Then update their lods
   const size_t trackCount = _tracks.size();
   for( size_t i = 0; i < trackCount; ++i )
   {
      const TimelineTrack& track = profiler.timelineTracks()[i];

      // Create the LOD for the traces
      const hop::Entries& traceEntries = track._traces.entries;
      appendLods( _tracks[i].traceLodsData, traceEntries );

      // Create LOD for the lockwaits
      const hop::Entries& lwEntries = track._lockWaits.entries;
      appendLods( _tracks[i].lockwaitsLodsData, lwEntries );

      // Update max depth as well in case it has changed
      _tracks[i].maxDepth = std::max( traceEntries.maxDepth, lwEntries.maxDepth );
   }

   // Finally update according to the options
   TRACE_HEIGHT = hop::options::traceHeight();
   PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

   const auto updateEnd = std::chrono::system_clock::now();
   hop::g_stats.updatingTimeMs +=
       std::chrono::duration<double, std::milli>( ( updateEnd - updateStart ) ).count();
}

void TimelineTracksView::draw( const TimelineTrackDrawData& data, TimelineMsgArray* msgArray )
{
   std::vector<HighlightInfo> highlightInfo;
   highlightInfo.reserve( 16 );

   drawTraceDetailsWindow( data, highlightInfo, msgArray );
   drawSearchWindow( data, highlightInfo, msgArray );
   drawTraceStats( _traceStats, data.profiler.stringDb(), data.timeline.useCycles );

   ImGui::SetCursorScreenPos( ImVec2( data.timeline.canvasPosX, data.timeline.canvasPosY ) );

   // Get data from profiler
   const std::vector<TimelineTrack>& timelineTracksData = data.profiler.timelineTracks();
   const StringDb& stringDb                             = data.profiler.stringDb();

   const float timelineOffsetY = data.timeline.canvasPosY + data.timeline.scrollAmount;
   assert( timelineTracksData.size() == _tracks.size() );
   for ( uint32_t i = 0; i < _tracks.size(); ++i )
   {
      // Skip empty threads
      const TimelineTrack& trackData = timelineTracksData[i];
      if( trackData.empty() ) continue;

      const bool threadHidden = hidden( i );
      const float trackHeight = trackHeightWithThreadLabel( i );

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const char* customName = trackData.name() != 0 ? stringDb.getString( stringDb.getStringIndex( trackData.name() ) ) : nullptr;

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();

      drawCoreLabels( labelsDrawPosition, data, i );

      if( drawThreadLabel( labelsDrawPosition, customName, i, threadHidden ) )
      {
         setTrackHeight( i, threadHidden ? 99999.0f : -99999.0f );
      }

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      _tracks[i].absoluteDrawPos[0] = absDrawPos.x;
      _tracks[i].absoluteDrawPos[1] = absDrawPos.y + data.timeline.scrollAmount - timelineOffsetY;
      _tracks[i].relativePosY       = absDrawPos.y; // Keeps the relative pos of the track for when resizing

      // Handle track resize
      if ( separatorHovered || _draggedTrack > 0 )
      {
         if ( _draggedTrack == -1 && ImGui::IsMouseClicked( 0 ) )
         {
            _draggedTrack = (int)i;
         }
         if( ImGui::IsMouseReleased( 0 ) )
         {
            _draggedTrack = -1;
         }
      }

      // Update the track height in case the max depth has changed
      setTrackHeight( i, _tracks[i].trackHeight );

      ImVec2 curDrawPos = absDrawPos;
      if( !threadHidden )
      {
         const float threadStartRelDrawPos = curDrawPos.y - ImGui::GetWindowPos().y;
         const float threadEndRelDrawPos = threadStartRelDrawPos + trackHeight;

         const bool tracesVisible =
             !( threadStartRelDrawPos > ImGui::GetWindowHeight() || threadEndRelDrawPos < 0 );

         if( tracesVisible )
         {
            // Track highlights needs to be drawn before the traces themselves as it acts as a background
            drawTrackHighlight(
                curDrawPos.x,
                curDrawPos.y - THREAD_LABEL_HEIGHT,
                trackHeight + THREAD_LABEL_HEIGHT );

            ImGui::PushClipRect(
                ImVec2( 0.0f, curDrawPos.y ),
                ImVec2( 9999.0f, curDrawPos.y + trackHeight ),
                true );

            // Draw the lock waits  entries (before traces so that they are not hiding them)
            const size_t lwHoveredIdx = drawEntries( curDrawPos, i, data, _tracks[i].lockwaitsLodsData, lockwaitLabelWithTime, getLockWaitZoneId );
            handleHoveredLockWait( *this, data, i, lwHoveredIdx, highlightInfo, msgArray );

            // Draw the traces entries
            const size_t traceHoveredIdx = drawEntries( curDrawPos, i, data, _tracks[i].traceLodsData, traceLabelWithTime, getTraceZoneId );
            handleHoveredTrace( _contextMenu, data, i, traceHoveredIdx, highlightInfo, msgArray );

            if( lwHoveredIdx == hop::INVALID_IDX && traceHoveredIdx == hop::INVALID_IDX )
            {
               // No trace were right clicked. Check for right click in canvas
               handleHoveredTrack( _contextMenu, _tracks, data );
            }

            // Finish by drawing all the highlighted entries if no context menu is open
            if( !_contextMenu.open )
               drawHighlightedTraces( _tracks, data, highlightInfo, data.highlightValue );

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
   }

   drawContextMenu( data );
}

void TimelineTracksView::drawSearchWindow(
   const hop::TimelineTrackDrawData& data,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   HOP_PROF_FUNC();
   using namespace hop;

   const auto& tracksData = data.profiler.timelineTracks();
   const SearchSelection selection = drawSearchResult( _searchResult, data.profiler.stringDb(), data.timeline, tracksData );

   if( selection.hoveredThreadIdx != (uint32_t)hop::INVALID_IDX && selection.hoveredTraceIdx != hop::INVALID_IDX )
   {
      const auto& timelinetrack    = tracksData[selection.hoveredThreadIdx];

      const TimeStamp absStartTime = timelinetrack._traces.entries.starts[selection.hoveredTraceIdx];
      const TimeStamp absEndTime   = timelinetrack._traces.entries.ends[selection.hoveredTraceIdx];
      const Depth_t depth          = timelinetrack._traces.entries.depths[selection.hoveredTraceIdx];

      traceToHighlight.emplace_back( HighlightInfo{ absStartTime, absEndTime, selection.hoveredThreadIdx, depth } );

      if ( selection.selectedTraceIdx != hop::INVALID_IDX && selection.selectedThreadIdx != (uint32_t)hop::INVALID_IDX )
      {
         assert( selection.selectedTraceIdx == selection.hoveredTraceIdx && selection.hoveredThreadIdx == selection.selectedThreadIdx );
         // If the thread was hidden, display it so we can see the selected trace
         setTrackHeight( selection.selectedThreadIdx, 9999.0f );

         const TimeStamp startTime = absStartTime - data.timeline.globalStartTime;
         const float verticalPosPxl = _tracks[selection.selectedThreadIdx].absoluteDrawPos[1] +
                                    ( depth * PADDED_TRACE_SIZE ) -
                                    ( 3 * PADDED_TRACE_SIZE );

         // Create the timeline messages ( frame horizontally and vertically )
         msgArray->addFrameTimeMsg( startTime, absEndTime - absStartTime, true, false /*abs time*/ );
         msgArray->addMoveVerticalPositionMsg( verticalPosPxl, true );
      }
   }
}

void TimelineTracksView::drawTraceDetailsWindow(
   const hop::TimelineTrackDrawData& data,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   const TraceDetailDrawResult traceDetailRes =
       drawTraceDetails( _traceDetails, data.profiler.timelineTracks(), data.profiler.stringDb(), data.timeline.useCycles );

   const auto& entries = data.profiler.timelineTracks()[traceDetailRes.hoveredThreadIdx]._traces.entries;
   if( traceDetailRes.clicked )
   {
      Depth_t minDepth = std::numeric_limits< Depth_t >::max();
      TimeStamp minTime = std::numeric_limits< TimeStamp >::max();
      TimeStamp maxTime = std::numeric_limits< TimeStamp >::min();
      for( size_t idx : traceDetailRes.hoveredTraceIds )
      {
         minDepth = std::min( entries.depths[ idx ], minDepth );
         maxTime = std::max( entries.ends[ idx ], maxTime );
         minTime = std::min( entries.starts[ idx ], minTime );
      }

      const float verticalPosPxl = _tracks[traceDetailRes.hoveredThreadIdx].absoluteDrawPos[1] +
                                   ( minDepth * PADDED_TRACE_SIZE ) -
                                   ( 3 * PADDED_TRACE_SIZE );

      // Create the timeline messages ( frame horizontally and vertically )
      msgArray->addFrameTimeMsg( minTime, maxTime - minTime, true, true );
      msgArray->addMoveVerticalPositionMsg( verticalPosPxl, true );
   }

   const size_t prevSize   = traceToHighlight.size();
   const size_t newElCount = traceDetailRes.hoveredTraceIds.size();
   traceToHighlight.resize( prevSize + newElCount );
   for( size_t i = 0; i < newElCount; ++i )
   {
      const size_t idx = traceDetailRes.hoveredTraceIds[i];
      traceToHighlight[prevSize+i].start     = entries.starts[ idx ];
      traceToHighlight[prevSize+i].end       = entries.ends[ idx ];
      traceToHighlight[prevSize+i].threadIdx = traceDetailRes.hoveredThreadIdx;
      traceToHighlight[prevSize+i].depth     = entries.depths[ idx ];
   }
}

bool TimelineTracksView::handleHotkeys()
{
   if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'f' ) )
   {
      _searchResult.searchWindowOpen = true;
      _searchResult.focusSearchWindow = true;
      return true;
   }

   return false;
}

bool TimelineTracksView::handleMouse( float, float posY, bool, bool, float )
{
   bool handled = false;
   if ( _draggedTrack > 0 )
   {
      // Find the previous track that is visible
      int i = _draggedTrack - 1;
      while ( i > 0 && _tracks[i].maxDepth > 0 )
      {
         --i;
      }

      const float trackHeight = ( posY - _tracks[i].relativePosY - THREAD_LABEL_HEIGHT );
      setTrackHeight( i, trackHeight );

     handled = true;
   }

   return handled;
}

void TimelineTracksView::setTrackHeight( uint32_t trackIdx, float height )
{
   auto& track = _tracks[ trackIdx ];
   track.trackHeight = hop::clamp( height, -PADDED_TRACE_SIZE, ((float)(track.maxDepth) + 1) * PADDED_TRACE_SIZE );
}

void TimelineTracksView::clear()
{
   _tracks.clear();
   resetContextMenu( _contextMenu );
   clearSearchResult( _searchResult );
   clearTraceStats( _traceStats );
   clearTraceDetails( _traceDetails );
   _draggedTrack = -1;
}

void TimelineTracksView::drawContextMenu( const TimelineTrackDrawData& data )
{
   if ( _contextMenu.open )
   {
       ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
       ImGui::SetNextWindowBgAlpha( 0.8f );  // Transparent background
       if ( ImGui::BeginPopupContextItem( CTXT_MENU_STR ) )
       {
          if( _contextMenu.traceClicked )
          {
             if ( ImGui::Selectable( "Trace Stats" ) )
             {
                _traceStats = createTraceStats(
                    data.profiler.timelineTracks()[_contextMenu.threadIndex]._traces,
                    _contextMenu.threadIndex,
                    _contextMenu.traceId );
             }
             else if ( ImGui::Selectable( "Profile Stack" ) )
             {
                _traceDetails = createTraceDetails(
                    data.profiler.timelineTracks()[_contextMenu.threadIndex]._traces,
                    _contextMenu.threadIndex,
                    _contextMenu.traceId );
             }
          }
          else
          {
             if ( ImGui::Selectable( "Profile Track" ) )
             {
                 hop::displayModalWindow( "Computing total trace size...", hop::MODAL_TYPE_NO_CLOSE );
                 const uint32_t tIdx = _contextMenu.threadIndex;
                 std::thread t( [ this, tIdx, dispTrace = data.profiler.timelineTracks()[tIdx]._traces.copy() ]() {
                    _traceDetails = createGlobalTraceDetails( dispTrace, tIdx );
                    closeModalWindow();
                 } );
                 t.detach();
             }
             else if ( ImGui::BeginMenu("Tracks") )
             {
                if( ImGui::Selectable( "Resize to Fit" ) )
                {
                   resizeAllTracksToFit();
                }
                else if( ImGui::Selectable( "Collapse" ) )
                {
                   setAllTracksCollapsed( true );
                }
                else if( ImGui::Selectable( "Expand" ) )
                {
                   setAllTracksCollapsed( false );
                }
                ImGui::EndMenu();
             }
          }
          ImGui::EndPopup();
       }
       else
       {
          // Reset the context menu info if not used anymore
          resetContextMenu( _contextMenu );
       }
       ImGui::PopStyleVar();
   }
}

void TimelineTracksView::resizeAllTracksToFit()
{
   float visibleTrackCount = 0;
   for( auto& i : _tracks )
      if( !i.traceLodsData.latestLodPerDepth.empty() ) ++visibleTrackCount;

   float timelineCanvasHeight = ImGui::GetIO().DisplaySize.y;

   const float totalTraceHeight = timelineCanvasHeight - visibleTrackCount * THREAD_LABEL_HEIGHT;
   const float heightPerTrack = totalTraceHeight / visibleTrackCount;

   // Autofit all track except the last one
   const size_t lastThread = _tracks.size() - 1;
   for( size_t i = 0; i < lastThread; ++i )
      setTrackHeight( i, heightPerTrack );

   setTrackHeight( lastThread, 9999.0f );
}

void TimelineTracksView::setAllTracksCollapsed( bool collapsed )
{
   const size_t trackCount = _tracks.size();
   const float heightVal = collapsed ? -9999.0f : 9999.0f;
   for( size_t i = 0; i < trackCount; ++i )
      setTrackHeight( i, heightVal );
}

}  // namespace hop