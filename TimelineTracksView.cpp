#include "common/Profiler.h"
#include "common/TimelineTrack.h"
#include "common/StringDb.h"
#include "common/Utils.h"

#include "Cursor.h"
#include "TimelineTracksView.h"
#include "TimelineInfo.h"
#include "Options.h"
#include "Stats.h"

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

static const char* getEntryName( const hop::Profiler& profiler, const hop::TimelineTrack& track, size_t index )
{
   return profiler.stringDb().getString( track._traces.fctNameIds[index] );
}

static int buildTraceLabelWithTime( const char* labelName, uint64_t duration, bool asCycles, uint32_t arrSz, char* arr)
{
   char fmtTime[32];
   hop::formatCyclesDurationToDisplay( duration, fmtTime, sizeof( fmtTime ), asCycles );

   return snprintf( arr, arrSz, "%s (%s)", labelName, fmtTime );
}

template< typename LodIt >
static void createDrawData( LodIt it, size_t count, hop::TimeStamp absStart, float cyclesPerPxl, float* __restrict startsPxl, float* __restrict deltaPxl )
{
   HOP_PROF_FUNC();
   for( size_t i = 0; i < count; ++i, ++it )
   {
      // Use the min max to clamp the starting position to 0 and remove what has been "cropped" from the length
      const auto minMaxPxl = std::minmax( (int64_t)( it->start - absStart ) / cyclesPerPxl, 0.0f );
      startsPxl[i] = minMaxPxl.second;
      deltaPxl[i]  = std::max( (( it->end - it->start ) / cyclesPerPxl) + minMaxPxl.first, 1.0f );
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

static void drawHoveredEntryPopup( size_t entryIndex, const hop::Profiler& profiler, const hop::TimelineTrack& track, bool asCycles )
{
   char strBuffer[512];
   const char* name = getEntryName( profiler, track, entryIndex );
   hop::TimeDuration delta = track._traces.entries.ends[ entryIndex ] - track._traces.entries.starts[ entryIndex ];
   const int charWritten = buildTraceLabelWithTime( name, delta, asCycles, sizeof( strBuffer ), strBuffer );

   snprintf(
       strBuffer + charWritten,
       std::max( 0, (int)sizeof( strBuffer ) - charWritten ),
       "\n   %s:%d ",
       profiler.stringDb().getString( track._traces.fileNameIds[entryIndex] ),
       track._traces.lineNbs[entryIndex] );

   ImGui::TextUnformatted( strBuffer );

// Print out some debug info as well
#ifdef HOP_DEBUG
   char strDebug[512];
   const auto end = track._traces.entries.ends[entryIndex];
   snprintf(
       strDebug,
       sizeof( strDebug ),
       "\n======== Debug Info ========\n"
       "Trace Index = %zu\n"
       "Trace Start = %zu\n"
       "Trace End   = %zu\n"
       "Trace Delta = %zu",
       entryIndex,
       end - delta,
       end,
       delta );
   ImGui::TextUnformatted( strDebug );
#endif
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
   const TimelineTrack& trackData = timelineTracksData[ threadIndex ];

   if ( trackData.empty() ) return;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   // Get all the timing boundaries
   const TimeStamp globalStartTime  = info.timeline.globalStartTime;
   const TimeStamp relativeStart    = info.timeline.relativeStartTime;
   const TimeDuration timelineRange = info.timeline.duration;

   const TimeStamp absoluteStart    = relativeStart + globalStartTime;
   const TimeStamp absoluteEnd      = absoluteStart + timelineRange;

   // The time range to draw in absolute time
   const LodsData& lodsData = info.drawInfos[threadIndex].lodsData;
   const auto spanIndex = hop::visibleIndexSpan( lodsData.lods, info.lodLevel, absoluteStart, absoluteEnd );

   if( spanIndex.first == hop::INVALID_IDX ) return;

   static std::vector< float > startPosPxl;
   static std::vector< float > deltaPxl;

   const size_t traceCount = spanIndex.second - spanIndex.first;
   startPosPxl.resize( traceCount );
   deltaPxl.resize( traceCount );

   const float windowWidthPxl = ImGui::GetWindowWidth();

   const auto lodStartIt = lodsData.lods[info.lodLevel].begin() + spanIndex.first;
   createDrawData( lodStartIt, traceCount, absoluteStart, timelineRange / windowWidthPxl, startPosPxl.data(), deltaPxl.data() );

   // Draw trace text left-aligned
   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2( 0.0f, 0.5f ) );
   const auto& zoneColors = g_options.zoneColors;

   size_t hoveredLodIdx = hop::INVALID_IDX;
   char entryName[256] = {};
   for( size_t i = 0; i < traceCount; ++i )
   {
      const LodInfo& curLod = *(lodStartIt + i);
      const size_t absIndex = curLod.index;

      // Create the name for the trace if it is large enough on screen
      entryName[0] = '\0';
      const float curDeltaPxl = deltaPxl[i];
      if( curDeltaPxl > 10.0f && !curLod.loded )
      {
         const char* name = getEntryName( info.profiler, trackData, absIndex );
         buildTraceLabelWithTime( name, curLod.end - curLod.start, false, sizeof(entryName), entryName );
      }

      const uint32_t zoneId = setBitIndex( trackData._traces.zones[absIndex] );
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, zoneColors[zoneId] );

      ImGui::SetCursorScreenPos( ImVec2( startPosPxl[i], posY + trackData._traces.entries.depths[absIndex] * info.paddedTraceHeight ) );
      ImGui::Button( entryName, ImVec2( curDeltaPxl, info.paddedTraceHeight ) );
      if( ImGui::IsItemHovered() && !curLod.loded )
      {
         hoveredLodIdx = i;
      }

      ImGui::PopStyleColor( 3 );
   }
   ImGui::PopStyleVar(); // Pop left-aligned

   if( hoveredLodIdx != hop::INVALID_IDX )
   {
      const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
      const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );
      const bool drawAsCycles = info.timeline.useCycles;

      ImGui::BeginTooltip();
      const size_t hoveredAbsIndex = (lodStartIt + hoveredLodIdx)->index;
      drawHoveredEntryPopup( hoveredAbsIndex, info.profiler, trackData, drawAsCycles );
      ImGui::EndTooltip();

      // Add the hovered trace to the highlighted traces
      info.drawInfos[threadIndex].highlightInfo.emplace_back(
         TrackHighlightInfo{
            startPosPxl[hoveredLodIdx],
            posY + trackData._traces.entries.depths[hoveredAbsIndex] * info.paddedTraceHeight,
            deltaPxl[hoveredLodIdx],
            0xFFFFFFFF } );

   /*
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
      */
   }

   startPosPxl.clear();
   deltaPxl.clear();

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.traceDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
}

static void drawHighlightedTraces(
    const std::vector<hop::TrackHighlightInfo>& highInfo,
    float highlightVal,
    float traceHeight )
{
   ImDrawList* drawList = ImGui::GetWindowDrawList();
   for( const auto& d : highInfo )
   {
      const uint32_t color = (d.color & 0x00FFFFFF) | (uint32_t(highlightVal * 255.0f) << 24);
      ImVec2 topLeft( d.posPxl[0], d.posPxl[1] );
      ImVec2 bottomRight( d.posPxl[0] + d.lengthPxl, d.posPxl[1] + traceHeight );

      drawList->AddRectFilled( topLeft, bottomRight, color );
   }
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

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
      drawLabels( info, labelsDrawPosition, threadLabel, i );

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      info.drawInfos[i].absoluteDrawPos[0] = absDrawPos.x;
      info.drawInfos[i].absoluteDrawPos[1] = absDrawPos.y + info.timeline.scrollAmount - timelineOffsetY;

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

            // Draw the lock waits (before traces so that they are not hiding them)
            // drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            drawTraces( i, curDrawPos.x, curDrawPos.y, info, msgArray );

            drawHighlightedTraces( info.drawInfos[i].highlightInfo, info.highlightValue, info.paddedTraceHeight );
            info.drawInfos[i].highlightInfo.clear();

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
   }

   //drawContextMenu( info );
}
