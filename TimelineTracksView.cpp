#include "common/Profiler.h"
#include "common/TimelineTrack.h"
#include "common/StringDb.h"
#include "common/Utils.h"

#include "Cursor.h"
#include "TimelineTracksView.h"
#include "TimelineInfo.h"
#include "Options.h"

#include "Lod.h"

#include "imgui/imgui.h"

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT       = 20.0f;
static constexpr uint32_t DISABLED_COLOR         = 0xFF505050;
static constexpr uint32_t SEPARATOR_COLOR        = 0xFF666666;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR = 0xFFAAAAAA;

static bool hidden( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
   return tdi.drawInfos[idx].trackHeight <= -tdi.paddedTraceHeight;
}

static float heightWithThreadLabel( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
    return tdi.drawInfos[idx].trackHeight + THREAD_LABEL_HEIGHT;
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

static void drawLabels(
    hop::TimelineTrackDrawInfo& info,
    const ImVec2& drawPosition,
    const char* threadName,
    uint32_t trackIndex/*,
    const hop::TimelineTracksDrawInfo& info*/ )
{
   const ImVec2 threadLabelSize = ImGui::CalcTextSize( threadName );
   ImGui::PushClipRect(
       ImVec2( drawPosition.x + threadLabelSize.x + 8, drawPosition.y ),
       ImVec2( 99999.0f, 999999.0f ),
       true );

   const bool trackHidden = hidden( info, trackIndex );
   const auto& zoneColors = hop::g_options.zoneColors;
   uint32_t threadLabelCol = zoneColors[( trackIndex + 1 ) % HOP_MAX_ZONE_COLORS];
   if( trackHidden )
   {
      threadLabelCol = DISABLED_COLOR;
   }
   else if( hop::g_options.showCoreInfo )
   {
      // Draw the core labels
      // drawCoresLabels(
      //     ImVec2( drawPosition.x, drawPosition.y + 1.0f ), tracks[trackIndex]._coreEvents, info );
      // Restore draw position for thread label
      //ImGui::SetCursorScreenPos( drawPosition );
   }

   ImGui::PopClipRect();

   // Draw thread label
   ImGui::PushID( trackIndex );
   ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
   if( ImGui::Button( threadName, ImVec2( 0, THREAD_LABEL_HEIGHT ) ) )
   {
      info.drawInfos[trackIndex].trackHeight = trackHidden ? 99999.0f : -99999.0f;
   }
   ImGui::PopStyleColor();
   ImGui::PopID();
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


// namespace
// {
//    struct DrawData
//    {
//       struct Entry
//       {
//          ImVec2 posPxl;
//          hop::TimeDuration duration;
//          size_t traceIndex;
//          float lengthPxl;
//       };
//       std::vector< Entry > entries;
//       union
//       {
//          const hop::TraceData*    tData;
//          const hop::LockWaitData* lwData;
//       } entryData;
//    };
// }
// static DrawData::Entry createDrawDataForEntry(
//     hop::TimeStamp traceEnd,
//     hop::TimeDuration traceDelta,
//     hop::Depth_t traceDepth,
//     size_t traceIdx,
//     const float posX,
//     const float posY,
//     const hop::TimelineTracksDrawInfo& drawInfo,
//     const float windowWidthPxl )
// {
//    using namespace hop;
//    const TimeStamp traceEndTime = ( traceEnd - drawInfo.timeline.globalStartTime );
//    const auto traceEndPxl = cyclesToPxl<float>(
//        windowWidthPxl,
//        drawInfo.timeline.duration,
//        traceEndTime - drawInfo.timeline.relativeStartTime );
//    const float traceLengthPxl = std::max(
//        MIN_TRACE_LENGTH_PXL,
//        cyclesToPxl<float>( windowWidthPxl, drawInfo.timeline.duration, traceDelta ) );

//    // Crop the trace that span outside the screen to make the text slides to be center
//    // with the trace
//    const float nonCroppedPosX = posX + traceEndPxl - traceLengthPxl;
//    const float croppedTracePosX = std::max( 0.0f, posX + traceEndPxl - traceLengthPxl );

//    // Get the amount cropped on each side of the screen and remove it from the total trace length
//    const float leftCroppedAmnt = croppedTracePosX - nonCroppedPosX;
//    const float rightCroppedAmnt =
//        hop::clamp( ( nonCroppedPosX + traceLengthPxl ) - windowWidthPxl, 0.0f, traceLengthPxl );
//    const float croppedTraceLenghtPxl = traceLengthPxl - (leftCroppedAmnt + rightCroppedAmnt);

//    const ImVec2 tracePos( croppedTracePosX, posY + traceDepth * TimelineTrack::PADDED_TRACE_SIZE );

//    return DrawData::Entry{tracePos, traceDelta, traceIdx, croppedTraceLenghtPxl};
// }

static int
getTraceLabel( size_t entryIndex, const hop::StringDb& strDb, uint32_t arrSz, char* arr )
{
   return snprintf( arr, arrSz, "Trace name" );
   // const size_t idx = ddEntry.traceIndex;
   // const char* entryName = strDb.getString( dd.entryData.tData->fctNameIds[idx] );
 
   // return buildTraceLabelWithTime( entryName,  ddEntry.duration, false, arrSz, arr );
}

static void convertDeltaCyclesToPxl( std::deque<hop::TimeDuration>::const_iterator first, std::deque<hop::TimeDuration>::const_iterator last, float windowWidth, hop::TimeStamp timelineRange, float* out )
{
   auto count = std::distance( first, last );
   const float cyclePerPxl = timelineRange / windowWidth;
   for( size_t i = 0; i < count; ++i, ++first )
   {
      out[i] = *first / cyclePerPxl;
   }
}

static void convertEndTimestampToStartPxlPos( std::deque<hop::TimeStamp>::const_iterator first, std::deque<hop::TimeStamp>::const_iterator last, hop::TimeStamp cycleOffset, float windowWidth, hop::TimeStamp timelineRange, const float* deltas, float* out )
{
   auto count = std::distance( first, last );
   const float cyclePerPxl = timelineRange / windowWidth;
   for( size_t i = 0; i < count; ++i, ++first )
   {
      out[i] = ((*first - cycleOffset) / cyclePerPxl) - deltas[i];
   }
}

static void drawTraces(
    uint32_t threadIndex,
    const float posX,
    const float posY,
    hop::TimelineTrackDrawInfo& info,
    hop::TimelineMsgArray* msgArray )
{
   using namespace hop;

   const std::vector<hop::TimelineTrack>& timelineTracksData = info.profiler.timelineTracks();
   const TimelineTrack& data = timelineTracksData[ threadIndex ];

   if ( data.empty() ) return;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   // Find the best lodLevel for our current zoom
   //const int lodLevel = _lodLevel;

   // Get all the timing boundaries
   const TimeStamp globalStartTime  = info.timeline.globalStartTime;
   const TimeStamp relativeStart    = info.timeline.relativeStartTime;
   const TimeDuration timelineRange = info.timeline.duration;

   const TimeStamp absoluteStart    = relativeStart + globalStartTime;
   const TimeStamp absoluteEnd      = absoluteStart + timelineRange;

    // The time range to draw in absolute time
   const auto spanIndex = hop::visibleIndexSpan( data._traces.entries, absoluteStart, absoluteEnd, 0 );

   if( spanIndex.first == hop::INVALID_IDX ) return;

   const size_t traceCount = spanIndex.second - spanIndex.first;
   static std::vector< float > startPosPxl;
   static std::vector< float > deltaPxl;

   startPosPxl.resize( traceCount );
   deltaPxl.resize( traceCount );

   const float windowWidthPxl = ImGui::GetWindowWidth();
   //const auto deltaBeginIt = data._traces.entries.deltas.begin();
   //convertDeltaCyclesToPxl( deltaBeginIt + spanIndex.first, deltaBeginIt + spanIndex.second, windowWidthPxl, timelineRange, deltaPxl.data() );

   LodsArray2 lods = computeLods2( data._traces.entries, 0 );

   const auto endsBeginIt = data._traces.entries.ends.begin();
   convertEndTimestampToStartPxlPos( endsBeginIt + spanIndex.first, endsBeginIt + spanIndex.second, absoluteStart, windowWidthPxl, timelineRange, deltaPxl.data(), startPosPxl.data() );

   size_t hoveredIdx = hop::INVALID_IDX;
   char entryName[256] = {};
   for( size_t i = 0; i < traceCount; ++i )
   {
      const char* name = "";
      const float curDeltaPxl = deltaPxl[i];
      if( curDeltaPxl > 10.0f )
      {
         getTraceLabel( i, info.profiler.stringDb(), sizeof(entryName), entryName );
      }

      ImGui::SetCursorScreenPos( ImVec2( startPosPxl[i], posY + data._traces.entries.depths[spanIndex.first + i] * info.paddedTraceHeight ) );
      ImGui::Button( entryName, ImVec2( curDeltaPxl, info.paddedTraceHeight ) );
      if( ImGui::IsItemHovered() )
      {
         hoveredIdx = i;
      }
   }

   startPosPxl.clear();
   deltaPxl.clear();

   //return hoveredIdx;

   // Gather draw data for all visible traces
   // HOP_PROF_SPLIT( "Creating draw data" );
   // for ( size_t i = spanLodIndex.first; i < spanLodIndex.second; ++i )
   // {
   //    const auto& t = data._traces.lods[lodLevel][i];
   //    const uint32_t zoneIndex = setBitIndex( data._traces.zones[t.traceIndex] );
   //    auto& lodToDraw = t.isLoded ? lodTracesToDraw : tracesToDraw;
   //    lodToDraw[zoneIndex].entries.push_back( createDrawDataForEntry(
   //           t.end, t.delta, t.depth, t.traceIndex, posX, posY, drawInfo, windowWidthPxl ) );
   // }
/*
   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;

   const DrawData* hoveredDrawData = nullptr;
   size_t hoveredIdx = hop::INVALID_IDX;

   // Draw trace text left-aligned
   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2( 0.0f, 0.5f ) );

   HOP_PROF_SPLIT( "Drawing Traces" );
   for ( size_t zoneId = 0; zoneId < lodTracesToDraw.size(); ++zoneId )
   {
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, zoneColors[zoneId] );
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );

      // Draw the lod traces
      drawEntries( lodTracesToDraw[ zoneId ], drawInfo.strDb, getEmptyLabel );

      // Draw the non-loded traces
      size_t curHoveredIdx = drawEntries( tracesToDraw[ zoneId ], drawInfo.strDb, getTraceLabel );
      if( curHoveredIdx != hop::INVALID_IDX )
      {
         hoveredIdx = curHoveredIdx;
         hoveredDrawData = &tracesToDraw[ zoneId ];
      }

      ImGui::PopStyleColor(3);
      ImGui::PopStyleVar();
   }
   ImGui::PopStyleVar(); // // Pop left-aligned

   if( hoveredIdx != hop::INVALID_IDX )
   {
      const bool drawAsCycles = drawInfo.timeline.useCycles;
      // Draw the tooltip for the hovered entry
      ImGui::BeginTooltip();
      drawHoveredEntryPopup(
          *hoveredDrawData, drawInfo.strDb, hoveredIdx, getTraceLabel, drawAsCycles );
      ImGui::EndTooltip();

      // Add the hovered trace to the highlighted traces
      addEntryToHighlight( _tracks[threadIndex], *hoveredDrawData, hoveredIdx );

      // Handle mouse interaction
      const DrawData::Entry& ddEntry = hoveredDrawData->entries[hoveredIdx];
      if( leftMouseDblClicked )
      {
         timelineMsg.emplace_back(
             createZoomOnEntryTimelineMsg( ddEntry, data._traces.entries, globalStartTime ) );
      }
      else if( rightMouseClicked && !drawInfo.timeline.mouseDragging )
      {
         ImGui::OpenPopup( CTXT_MENU_STR );
         _contextMenuInfo.open = true;
         _contextMenuInfo.traceClick = true;
         _contextMenuInfo.threadIndex = threadIndex;
         _contextMenuInfo.traceId = ddEntry.traceIndex;
      }
   }

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.traceDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
   */
}

void hop::drawTimelineTracks( TimelineTrackDrawInfo& info, TimelineMsgArray* msgArray )
{
   //drawTraceDetailsWindow( info, timelineActions );
   //drawSearchWindow( info, timelineActions );
   //drawTraceStats( _traceStats, info.strDb, info.timeline.useCycles );

   ImGui::SetCursorScreenPos( ImVec2( info.timeline.canvasPosX, info.timeline.canvasPosY ) );

   // Get data from profiler
   const std::vector<TimelineTrack>& timelineTracksData = info.profiler.timelineTracks();
   const StringDb& stringDb                             = info.profiler.stringDb();

   char threadNameBuffer[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread" );
   const float timelineOffsetY = info.timeline.canvasPosY + info.timeline.scrollAmount;
   assert( timelineTracksData.size() == info.drawInfos.size() );
   for ( size_t i = 0; i < info.drawInfos.size(); ++i )
   {
      // Skip empty threads
      const TimelineTrack& trackData = timelineTracksData[i];
      if( trackData.empty() ) continue;

      const bool threadHidden = hidden( info, i );
      const float trackHeight = heightWithThreadLabel( info, i );

      const char* threadLabel = &threadNameBuffer[0];
      hop::StrPtr_t trackname = trackData.name();
      if( trackname != 0 )
      {
         const size_t stringIdx = stringDb.getStringIndex( trackname );
         threadLabel = stringDb.getString( stringIdx );
      }
      else
      {
         snprintf(
             threadNameBuffer + threadNamePrefix, sizeof( threadNameBuffer ) - threadNamePrefix, "%lu", i );
      }
      HOP_PROF_DYN_NAME( threadLabel );

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
      drawLabels( info, labelsDrawPosition, threadLabel, i );

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      info.drawInfos[i].absoluteDrawPos[0] = absDrawPos.x;
      info.drawInfos[i].absoluteDrawPos[1] = absDrawPos.y + info.timeline.scrollAmount - timelineOffsetY;
      info.drawInfos[i].localDrawPos[0] = absDrawPos.x;
      info.drawInfos[i].localDrawPos[1] = absDrawPos.y;

      // Handle track resize
      if ( separatorHovered || info.draggedTrack > 0 )
      {
         if ( info.draggedTrack == -1 && ImGui::IsMouseClicked( 0 ) )
         {
            info.draggedTrack = (int)i;
         }
         if( ImGui::IsMouseReleased( 0 ) )
         {
            info.draggedTrack = -1;
         }
      }


      ImVec2 curDrawPos = absDrawPos;
      if (!threadHidden)
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

            // Draw the lock waits (before traces so that they are not hiding them)
            // drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            drawTraces( i, curDrawPos.x, curDrawPos.y, info, msgArray );

            // drawHighlightedTraces(
            //     _tracks[i]._highlightsDrawData,
            //     _highlightValue );
            // _tracks[i]._highlightsDrawData.clear();

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
      //*/
   }

   //drawContextMenu( info );
}
