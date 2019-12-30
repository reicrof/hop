#include "common/Profiler.h"
#include "common/TimelineTrack.h"
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

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT       = 20.0f;
static constexpr uint32_t DISABLED_COLOR         = 0xFF505050;
static constexpr uint32_t SEPARATOR_COLOR        = 0xFF666666;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR = 0xFFAAAAAA;
static const char* CTXT_MENU_STR = "Context Menu";

// Static variable mutable from options
static float TRACE_HEIGHT = 20.0f;
static float TRACE_VERTICAL_PADDING = 2.0f;
static float PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

struct HighlightInfo
{
   uint32_t threadIdx;
   size_t traceIdx;
};

static void resetContextMenu( hop::TimelineTracksView::ContextMenu& ctxt )
{
   memset( &ctxt, 0, sizeof( ctxt ) );
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
      snprintf( nameBuffer, sizeof( nameBuffer ), "Thread %lu", trackIndex );
   }

   const ImVec2 threadLabelSize = ImGui::CalcTextSize( nameBuffer );
   ImGui::PushClipRect(
       ImVec2( drawPosition.x + threadLabelSize.x + 8, drawPosition.y ),
       ImVec2( 99999.0f, 999999.0f ),
       true );

   const auto& zoneColors = hop::g_options.zoneColors;
   uint32_t threadLabelCol = zoneColors[( trackIndex + 1 ) % HOP_MAX_ZONE_COLORS];
   if( hidden )
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

static size_t drawTraces(
    uint32_t threadIndex,
    const float posX,
    const float posY,
    std::vector<hop::TimelineTracksView::TrackViewData>& tracksView,
    const hop::TimelineTrackDrawData& data )
{
   using namespace hop;

   const std::vector<hop::TimelineTrack>& timelineTracksData = data.profiler.timelineTracks();
   const TimelineTrack& trackData = timelineTracksData[ threadIndex ];

   if ( trackData.empty() ) return hop::INVALID_IDX;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   // Get all the timing boundaries
   const TimeStamp globalStartTime  = data.timeline.globalStartTime;
   const TimeStamp relativeStart    = data.timeline.relativeStartTime;
   const TimeDuration timelineRange = data.timeline.duration;

   const TimeStamp absoluteStart    = relativeStart + globalStartTime;
   const TimeStamp absoluteEnd      = absoluteStart + timelineRange;

   // The time range to draw in absolute time
   const LodsData& lodsData = tracksView[threadIndex].lodsData;
   const auto spanIndex = hop::visibleIndexSpan( lodsData.lods, data.lodLevel, absoluteStart, absoluteEnd );

   if( spanIndex.first == hop::INVALID_IDX ) return hop::INVALID_IDX;

   static std::vector< float > startPosPxl;
   static std::vector< float > deltaPxl;

   const size_t traceCount = spanIndex.second - spanIndex.first;
   startPosPxl.resize( traceCount );
   deltaPxl.resize( traceCount );

   const float windowWidthPxl = ImGui::GetWindowWidth();

   const auto lodStartIt = lodsData.lods[data.lodLevel].begin() + spanIndex.first;
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
      if( curDeltaPxl > 5.0f && !curLod.loded )
      {
         const char* name = getEntryName( data.profiler, trackData, absIndex );
         buildTraceLabelWithTime( name, curLod.end - curLod.start, false, sizeof(entryName), entryName );
      }

      const uint32_t zoneId = setBitIndex( trackData._traces.zones[absIndex] );
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, zoneColors[zoneId] );

      ImGui::SetCursorScreenPos( ImVec2( startPosPxl[i], posY + trackData._traces.entries.depths[absIndex] * PADDED_TRACE_SIZE ) );
      ImGui::Button( entryName, ImVec2( curDeltaPxl, PADDED_TRACE_SIZE ) );
      if( ImGui::IsItemHovered() && !curLod.loded )
      {
         hoveredLodIdx = i;
      }

      ImGui::PopStyleColor( 3 );
   }
   ImGui::PopStyleVar(); // Pop left-aligned

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
      const auto& tracksData        = data.profiler.timelineTracks()[trace.threadIdx];
      const hop::TimeStamp start    = tracksData._traces.entries.starts[trace.traceIdx];
      const hop::TimeStamp end      = tracksData._traces.entries.ends[trace.traceIdx];
      const hop::Depth_t depth      = tracksData._traces.entries.depths[trace.traceIdx];
      const hop::TimeDuration delta = end - start;

      const float startPxl = hop::cyclesToPxl<float>( wndWidth, tlDuration, start - tlStart );
      const float deltaPxl = hop::cyclesToPxl<float>( wndWidth, tlDuration, delta );
      const float posYPxl  =
         data.timeline.canvasPosY + tracksView[trace.threadIdx].absoluteDrawPos[1] + depth * PADDED_TRACE_SIZE;

      ImVec2 topLeft( startPxl, posYPxl );
      drawList->AddRectFilled( topLeft, topLeft + ImVec2( deltaPxl, PADDED_TRACE_SIZE ), color );
   }
}

static void handleHoveredTrace(
   hop::TimelineTracksView::ContextMenu& contextMenu,
   const std::vector<hop::TimelineTracksView::TrackViewData>& tracksView,
   const hop::TimelineTrackDrawData& data,
   unsigned threadIndex,
   size_t hoveredIdx,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   if( hoveredIdx != hop::INVALID_IDX )
   {
      const bool drawAsCycles = data.timeline.useCycles;
      const hop::TimelineTrack& trackData = data.profiler.timelineTracks()[threadIndex];

      ImGui::BeginTooltip();
      drawHoveredEntryPopup( hoveredIdx, data.profiler, trackData, drawAsCycles );
      ImGui::EndTooltip();

      const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );
      // Handle mouse interaction
      if( leftMouseDblClicked )
      {
         const hop::TimeStamp start = trackData._traces.entries.starts[hoveredIdx];
         const hop::TimeStamp end = trackData._traces.entries.ends[hoveredIdx];
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

      traceToHighlight.emplace_back(HighlightInfo{ threadIndex, hoveredIdx });
   }

   // No trace were right clicked. Check for right click in canvas
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
      const hop::Entries& entries = profiler.timelineTracks()[i]._traces.entries;
      appendLods( _tracks[i].lodsData, entries );

      // Update max depth as well in case it has changed
      _tracks[i].maxDepth = entries.maxDepth;
   }

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

      //drawCoreLabels(stuff)
      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
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

            // Draw the lock waits (before traces so that they are not hiding them)
            // drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            const size_t hoveredIdx = drawTraces( i, curDrawPos.x, curDrawPos.y, _tracks, data );

            handleHoveredTrace( _contextMenu, _tracks, data, i, hoveredIdx, highlightInfo, msgArray );

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

   if ( selection.selectedTraceIdx != (size_t)-1 && selection.selectedThreadIdx != (uint32_t)-1 )
   {
      const auto& timelinetrack    = tracksData[selection.selectedThreadIdx];
      const size_t traceIdx        = selection.selectedTraceIdx;

      const TimeStamp absStartTime = timelinetrack._traces.entries.starts[traceIdx];
      const TimeStamp absEndTime   = timelinetrack._traces.entries.ends[traceIdx];
      const Depth_t depth          = timelinetrack._traces.entries.depths[traceIdx];

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

   if( selection.hoveredThreadIdx != hop::INVALID_IDX && selection.hoveredTraceIdx != hop::INVALID_IDX )
   {
      traceToHighlight.emplace_back( HighlightInfo{ selection.hoveredThreadIdx, selection.hoveredTraceIdx } );
   }
}

void TimelineTracksView::drawTraceDetailsWindow(
   const hop::TimelineTrackDrawData& data,
   std::vector<HighlightInfo>& traceToHighlight,
   hop::TimelineMsgArray* msgArray )
{
   const TraceDetailDrawResult traceDetailRes =
       drawTraceDetails( _traceDetails, data.profiler.timelineTracks(), data.profiler.stringDb(), data.timeline.useCycles );

   if( traceDetailRes.clicked )
   {
      Depth_t minDepth = std::numeric_limits< Depth_t >::max();
      TimeStamp minTime = std::numeric_limits< TimeStamp >::max();
      TimeStamp maxTime = std::numeric_limits< TimeStamp >::min();
      const auto& timelinetrack = data.profiler.timelineTracks()[traceDetailRes.hoveredThreadIdx];
      for( size_t idx : traceDetailRes.hoveredTraceIds )
      {
         minDepth = std::min( timelinetrack._traces.entries.depths[ idx ], minDepth );
         maxTime = std::max( timelinetrack._traces.entries.ends[ idx ], maxTime );
         minTime = std::min( timelinetrack._traces.entries.starts[ idx ], minTime );
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
   traceToHighlight.resize(
      prevSize + newElCount, HighlightInfo{ traceDetailRes.hoveredThreadIdx, 0 } );
   for( size_t i = 0; i < newElCount; ++i )
   {
      traceToHighlight[prevSize+i].traceIdx = traceDetailRes.hoveredTraceIds[i];
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

bool TimelineTracksView::handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel )
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
      if( !i.lodsData.latestLodPerDepth.empty() ) ++visibleTrackCount;

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