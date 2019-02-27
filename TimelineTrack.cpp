#include "TimelineTrack.h"
#include "TimelineInfo.h"
#include "Lod.h"
#include "Utils.h"
#include "Options.h"
#include "Cursor.h"
#include "Stats.h"
#include "StringDb.h"
#include "ModalWindow.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cassert>
#include <cmath> // std::sin
#include <cstring> // memcpy
#include <thread>

static constexpr float THREAD_LABEL_HEIGHT = 20.0f;
static constexpr float MIN_TRACE_LENGTH_PXL = 1.0f;
static constexpr float MAX_TRACE_HEIGHT = 50.0f;
static constexpr float MIN_TRACE_HEIGHT = 15.0f;
static constexpr uint32_t DISABLED_COLOR = 0xFF505050;
static constexpr uint32_t CORE_LABEL_COLOR = 0xFF333333;
static constexpr uint32_t CORE_LABEL_BORDER_COLOR = 0xFFAAAAAA;
static constexpr uint32_t SEPARATOR_COLOR = 0xFF666666;

static const char* CTXT_MENU_STR = "Context Menu";

namespace
{
   struct DrawData
   {
      struct Entry
      {
         ImVec2 posPxl;
         hop::TimeDuration duration;
         size_t traceIndex;
         float lengthPxl;
      };
      std::vector< Entry > entries;
      union
      {
         const hop::TraceData*    tData;
         const hop::LockWaitData* lwData;
      } entryData;
   };
}  // namespace

static DrawData::Entry createDrawDataForEntry(
    hop::TimeStamp traceEnd,
    hop::TimeDuration traceDelta,
    hop::Depth_t traceDepth,
    size_t traceIdx,
    const float posX,
    const float posY,
    const hop::TimelineTracksDrawInfo& drawInfo,
    const float windowWidthPxl )
{
   using namespace hop;
   const TimeStamp traceEndTime = ( traceEnd - drawInfo.timeline.globalStartTime );
   const auto traceEndPxl = cyclesToPxl<float>(
       windowWidthPxl,
       drawInfo.timeline.duration,
       traceEndTime - drawInfo.timeline.relativeStartTime );
   const float traceLengthPxl = std::max(
       MIN_TRACE_LENGTH_PXL,
       cyclesToPxl<float>( windowWidthPxl, drawInfo.timeline.duration, traceDelta ) );

   // Crop the trace that span outside the screen to make the text slides to be center
   // with the trace
   const float nonCroppedPosX = posX + traceEndPxl - traceLengthPxl;
   const float croppedTracePosX = std::max( 0.0f, posX + traceEndPxl - traceLengthPxl );

   // Get the amount cropped on each side of the screen and remove it from the total trace length
   const float leftCroppedAmnt = croppedTracePosX - nonCroppedPosX;
   const float rightCroppedAmnt =
       hop::clamp( ( nonCroppedPosX + traceLengthPxl ) - windowWidthPxl, 0.0f, traceLengthPxl );
   const float croppedTraceLenghtPxl = traceLengthPxl - (leftCroppedAmnt + rightCroppedAmnt);

   const ImVec2 tracePos( croppedTracePosX, posY + traceDepth * TimelineTrack::PADDED_TRACE_SIZE );

   return DrawData::Entry{tracePos, traceDelta, traceIdx, croppedTraceLenghtPxl};
}

static bool drawSeparator( uint32_t threadIndex, bool highlightSeparator )
{
   const float drawPosY = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;
   ImVec2 p1 = ImGui::GetWindowPos();
   p1.y += drawPosY;

   ImVec2 p2 = p1;
   p2.x += ImGui::GetWindowSize().x;
   p2.y += + 1;

   const bool hovered = std::abs( ImGui::GetMousePos().y - p1.y ) < 7.0f && threadIndex > 0;

   uint32_t color = SEPARATOR_COLOR;
   if( hovered && highlightSeparator )
   {
      hop::setCursor( hop::CURSOR_SIZE_NS );
      color = 0xFFFFFFFF;
   }

   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddLine( p1, p2, color, 3.0f );

   ImGui::SetCursorPosY( ImGui::GetCursorPosY() );

   return hovered && highlightSeparator ;
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

static void drawHighlightedTraces(
    const std::vector<hop::TimelineTrack::HighlightDrawInfo>& highlightDrawData,
    float highlightVal )
{
   ImDrawList* drawList = ImGui::GetWindowDrawList();
   for( const auto& t : highlightDrawData )
   {
      const uint32_t color = (t.color & 0x00FFFFFF) | (uint32_t(highlightVal * 255.0f) << 24);
      ImVec2 topLeft( t.posPxlX, t.posPxlY );
      ImVec2 bottomRight( t.posPxlX + t.lengthPxl, t.posPxlY + hop::TimelineTrack::TRACE_HEIGHT );

      drawList->AddRectFilled( topLeft, bottomRight, color );
   }
}

static void drawCoresLabels(
    const ImVec2& drawPos,
    const hop::CoreEventData& coreData,
    const hop::TimelineTracksDrawInfo& di )
{
   if( coreData.data.empty() ) return;

   const auto drawStart = std::chrono::system_clock::now();

   using namespace hop;

   const auto absoluteStart = di.timeline.globalStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + di.timeline.relativeStartTime;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + di.timeline.duration;

   CoreEvent firstEv = { firstTraceAbsoluteTime, firstTraceAbsoluteTime, 0 };
   CoreEvent lastEv = { lastTraceAbsoluteTime, lastTraceAbsoluteTime, 0 };
   auto cmp = []( const CoreEvent& lhs, const CoreEvent& rhs) { return lhs.end < rhs.end; };
   auto it1 = std::lower_bound( coreData.data.begin(), coreData.data.end(), firstEv, cmp );
   auto it2 = std::upper_bound( coreData.data.begin(), coreData.data.end(), lastEv, cmp );

   if( it2 != coreData.data.end() ) ++it2;
   if( it2 != coreData.data.end() ) ++it2;

   if( it1 == it2 ) return; // Nothing to draw here

   std::vector<DrawData::Entry> drawData;
   drawData.reserve( std::distance( it1, it2 ) );

   const uint64_t minCycleToMerge = hop::pxlToCycles( windowWidthPxl, di.timeline.duration, 10 );
   const uint64_t minCycleToSkip = hop::pxlToCycles( windowWidthPxl, di.timeline.duration, 0.5f );
   CoreEvent prevEvent = *it1;
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
         drawData.push_back( createDrawDataForEntry(
             prevEvent.end,
             labelDuration,
             0,
             prevEvent.core,
             drawPos.x,
             drawPos.y,
             di,
             windowWidthPxl ) );
      }

      prevEvent = *it1;
   }

   // Add the last entry to draw
   drawData.push_back( createDrawDataForEntry(
       prevEvent.end,
       prevEvent.end - prevEvent.start,
       0,
       prevEvent.core,
       drawPos.x,
       drawPos.y,
       di,
       windowWidthPxl ) );

   char curName[32] = "Core ";
   const int prefixSize = strlen( curName );

   ImGui::PushStyleColor( ImGuiCol_Button, CORE_LABEL_COLOR );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, CORE_LABEL_COLOR );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, CORE_LABEL_COLOR );
   ImGui::PushStyleColor( ImGuiCol_Border, CORE_LABEL_BORDER_COLOR );
   ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1.0f );
   for( const auto& t : drawData )
   {
      snprintf(
          curName + prefixSize, sizeof( curName ) - prefixSize, "%u", (uint32_t)t.traceIndex );
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( curName, ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT - 1 ) );
      if( ImGui::IsItemHovered() )
      {
         ImGui::BeginTooltip();
         ImGui::TextUnformatted( curName );
         ImGui::EndTooltip();
      }
   }
   ImGui::PopStyleVar(1);
   ImGui::PopStyleColor(4);

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.coreDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
}

static void drawLabels(
    const ImVec2& drawPosition,
    const char* threadName,
    std::vector<hop::TimelineTrack>& tracks,
    uint32_t trackIndex,
    const hop::TimelineTracksDrawInfo& info )
{
   const ImVec2 threadLabelSize = ImGui::CalcTextSize( threadName );
   ImGui::PushClipRect(
       ImVec2( drawPosition.x + threadLabelSize.x + 8, drawPosition.y ),
       ImVec2( 99999.0f, 999999.0f ),
       true );

   const bool threadHidden = tracks[trackIndex].hidden();
   const auto& zoneColors = hop::g_options.zoneColors;
   uint32_t threadLabelCol = zoneColors[( trackIndex + 1 ) % HOP_MAX_ZONE_COLORS];
   if( threadHidden )
   {
      threadLabelCol = DISABLED_COLOR;
   }
   else if( hop::g_options.showCoreInfo )
   {
      // Draw the core labels
      drawCoresLabels(
          ImVec2( drawPosition.x, drawPosition.y + 1.0f ), tracks[trackIndex]._coreEvents, info );
      // Restore draw position for thread label
      ImGui::SetCursorScreenPos( drawPosition );
   }

   ImGui::PopClipRect();

   // Draw thread label
   ImGui::PushID( trackIndex );
   ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
   if( ImGui::Button( threadName, ImVec2( 0, THREAD_LABEL_HEIGHT ) ) )
   {
      tracks[trackIndex].setTrackHeight( tracks[trackIndex].hidden() ? 99999.0f : -99999.0f );
   }
   ImGui::PopStyleColor();
   ImGui::PopID();
}

// Fct pointer to get entry name
typedef const char* (*EntryNameFct)(const DrawData& dd, size_t entryIndex, const hop::StringDb& strDb);

// Get entry name for non-loded traces
static const char*
nonLodTraceName( const DrawData& dd, size_t entryIndex, const hop::StringDb& strDb )
{
   return strDb.getString( dd.entryData.tData->fctNameIds[entryIndex] );
}

typedef void (*HoveredStringFct)(const DrawData& dd,
    size_t entryIndex,
    const hop::StringDb& strDb,
    char* buffer,
    uint32_t bufferSize);

static void getHoveredTraceInformation(
    const DrawData& dd,
    size_t entryIndex,
    const hop::StringDb& strDb,
    char* buffer,
    uint32_t bufferSize )
{
   char fmtTime[32];
   hop::formatCyclesDurationToDisplay(
       dd.entries[entryIndex].duration, fmtTime, sizeof( fmtTime ), /*drawAsCycles*/ false );

   int writeSize = snprintf(
       buffer,
       bufferSize,
       " (%s)\n   %s:%d ",
       fmtTime,
       strDb.getString( dd.entryData.tData->fileNameIds[entryIndex] ),
       dd.entryData.tData->lineNbs[entryIndex] );

   (void)(writeSize);
#ifdef HOP_DEBUG
   const auto end = dd.entryData.tData->entries.ends[entryIndex];
   const auto delta = dd.entryData.tData->entries.deltas[entryIndex];
   snprintf(
       buffer,
       bufferSize - writeSize,
       "\n======== Debug Info ========\n"
       "Trace Index = %zu\n"
       "Trace Start = %zu\n"
       "Trace End   = %zu\n"
       "Trace Delta = %zu",
       entryIndex,
       end - delta,
       end,
       delta );
#endif
}

static void drawNonLodEntries(
    const DrawData& dd,
    const hop::StringDb& strDb,
    EntryNameFct getEntryName,
    HoveredStringFct getHoveredString )
{
    char strBuffer[512] = "";
    for( const auto& t : dd.entries )
    {
       const size_t traceIndex = t.traceIndex;
       const char* entryName = getEntryName( dd, traceIndex, strDb );

       ImGui::SetCursorScreenPos( t.posPxl );
       ImGui::Button( entryName, ImVec2( t.lengthPxl, hop::TimelineTrack::TRACE_HEIGHT ) );
       if( ImGui::IsItemHovered() )
       {
          if( t.lengthPxl > 3 )
          {
               int nameLen = snprintf( strBuffer, sizeof(strBuffer), "%s ", entryName );
               getHoveredString(
                   dd, traceIndex, strDb, strBuffer + nameLen, sizeof( strBuffer ) - nameLen );
               ImGui::BeginTooltip();
               ImGui::TextUnformatted( strBuffer );
               ImGui::EndTooltip();
          }

//          // Highlight hovered trace
//          _tracks[threadIndex]._highlightsDrawData.emplace_back(
//              TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

//          if( leftMouseDblClicked )
//          {
//             const TimeStamp startTime = data._traces.entries.ends[traceIndex] -
//                                         data._traces.entries.deltas[traceIndex] - globalStartTime;
//             TimelineMessage msg;
//             msg.type = TimelineMessageType::FRAME_TO_TIME;
//             msg.frameToTime.time = startTime;
//             msg.frameToTime.duration = t.duration;
//             msg.frameToTime.pushNavState = true;

//             timelineMsg.push_back( msg );
//          }
//          else if( rightMouseClicked && !drawInfo.timeline.mouseDragging )
//          {
//             ImGui::OpenPopup( CTXT_MENU_STR );
//             _contextMenuInfo.open = true;
//             _contextMenuInfo.traceClick = true;
//             _contextMenuInfo.threadIndex = threadIndex;
//             _contextMenuInfo.traceId = t.traceIndex;
//          }
       }
    }
}

namespace hop
{

float TimelineTrack::TRACE_HEIGHT = 20.0f;
float TimelineTrack::TRACE_VERTICAL_PADDING = 2.0f;
float TimelineTrack::PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

void TimelineTrack::setTrackName( StrPtr_t name ) noexcept
{
   _trackName = name;
}

StrPtr_t TimelineTrack::trackName() const noexcept
{
   return _trackName;
}

void TimelineTrack::addTraces( const TraceData& newTraces )
{
   HOP_PROF_FUNC();

   // Check if the user has manually changed the track size
   const bool customTrackSize = height() < (maxDepth() + 1) * PADDED_TRACE_SIZE;

   _traces.append( newTraces );

   // If the track was not manually resized, set it to its new size
   if( !customTrackSize )
   {
      setTrackHeight( (maxDepth() + 1) * PADDED_TRACE_SIZE );
   }

   assert_is_sorted( _traces.entries.ends.begin(), _traces.entries.ends.end() );
}

void TimelineTrack::addLockWaits( const LockWaitData& lockWaits )
{
   HOP_PROF_FUNC();
   const size_t prevSize = _lockWaits.mutexAddrs.size();
   _lockWaits.append( lockWaits );

   for( size_t i = 0; i < lockWaits.mutexAddrs.size(); ++i )
   {
      _lockWaitsPerMutex[ lockWaits.mutexAddrs[i] ].push_back( prevSize + i );
   }
}

void TimelineTrack::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
   // If we did not get any lock events prior to the unlock events, simply ignore them
   if( _lockWaits.entries.ends.empty() ) return;

   HOP_PROF_FUNC();
   for( const auto& ue : unlockEvents )
   {
      // Find the list of lockwaits that have not yet been associated with
      // an unlock events for a specific mutex
      const auto lockWaitsIdx = _lockWaitsPerMutex.find( ue.mutexAddress );
      if(lockWaitsIdx != _lockWaitsPerMutex.end() )
      {
         std::vector< TimeStamp >& lockwaitIdx = lockWaitsIdx->second;
         size_t i = 0;
         for (; i < lockwaitIdx.size(); ++i)
         {
            if( _lockWaits.entries.ends[lockwaitIdx[i]] < ue.time )
            {
               _lockWaits.lockReleases[lockwaitIdx[i]] = ue.time;
               break;
            }
         }
         // If we found a lockwait that is associted with a specific unlock events,
         // all prior lockwaits can be dismiss and are either already associated or
         // their unlock event was dropped
         if( i++ != lockwaitIdx.size() )
            lockwaitIdx.erase( lockwaitIdx.begin(), lockwaitIdx.begin() + i );
      }
   }
}

void TimelineTrack::addCoreEvents( const std::vector<CoreEvent>& coreEvents )
{
   _coreEvents.data.insert( _coreEvents.data.end(), coreEvents.begin(), coreEvents.end() );
   assert_is_sorted( _coreEvents.data.begin(), _coreEvents.data.end() );
}

Depth_t TimelineTrack::maxDepth() const noexcept
{
   return _traces.entries.maxDepth;
}

bool TimelineTrack::hidden() const noexcept
{
   return _trackHeight <= -PADDED_TRACE_SIZE;
}

float TimelineTrack::height() const noexcept
{
   return _trackHeight;
}

float TimelineTrack::heightWithThreadLabel() const noexcept
{
   return _trackHeight + THREAD_LABEL_HEIGHT;
}

void TimelineTrack::setTrackHeight( float height )
{
   _trackHeight = hop::clamp( height, -PADDED_TRACE_SIZE, (float)(maxDepth() + 1) * PADDED_TRACE_SIZE );
}

bool TimelineTrack::empty() const
{
   return _traces.entries.ends.empty();
}

size_t serializedSize( const TimelineTrack& ti )
{
   return serializedSize( ti._traces ) + serializedSize( ti._lockWaits );
}

size_t serialize( const TimelineTrack& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    size_t i = 0;

    i += serialize( ti._traces, &data[i] );
    i += serialize( ti._lockWaits, &data[i] );
   
    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, TimelineTrack& ti )
{
    size_t i = 0;

    i += deserialize( &data[i], ti._traces );
    i += deserialize( &data[i], ti._lockWaits );

    return i;
}

bool TimelineTracks::handleMouse(
    float /*mousePosX*/,
    float mousePosY,
    bool /*lmClicked*/,
    bool /*rmClicked*/,
    float /*mousewheel*/ )
{
   bool handled = false;
   if ( _draggedTrack > 0 )
   {
      // Find the previous track that is visible
      int i = _draggedTrack - 1;
      while ( i > 0 && _tracks[i].empty() )
      {
         --i;
      }

      const float trackHeight = ( mousePosY - _tracks[i]._localDrawPos[1] - THREAD_LABEL_HEIGHT );
      _tracks[i].setTrackHeight( trackHeight );

      handled = true;
   }

   return handled;
}

bool TimelineTracks::handleHotkey()
{
   if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'f' ) )
   {
      _searchRes.searchWindowOpen = true;
      _searchRes.focusSearchWindow = true;
      return true;
   }

   return false;
}

void TimelineTracks::update( float deltaTimeMs, TimeDuration timelineDuration )
{
   // Update the highlight factor
   static float x = 0.0f;
   x += 0.007f * deltaTimeMs;
   _highlightValue = (std::sin( x ) * 0.8f + 1.0f) / 2.0f;

   // Update current lod level
   _lodLevel = 0;
   while ( _lodLevel < LOD_COUNT - 1 && timelineDuration > LOD_NANOS[_lodLevel] )
   {
      ++_lodLevel;
   }

   // Update according to options
   TimelineTrack::TRACE_HEIGHT = hop::clamp( g_options.traceHeight, MIN_TRACE_HEIGHT, MAX_TRACE_HEIGHT );
   TimelineTrack::PADDED_TRACE_SIZE = TimelineTrack::TRACE_HEIGHT + TimelineTrack::TRACE_VERTICAL_PADDING;
}

std::vector< TimelineMessage > TimelineTracks::draw( const TimelineTracksDrawInfo& info )
{
   std::vector< TimelineMessage > timelineActions;
   timelineActions.reserve( 4 );

   drawTraceDetailsWindow( info, timelineActions );
   drawSearchWindow( info, timelineActions );
   drawTraceStats( _traceStats, info.strDb, info.timeline.useCycles );

   ImGui::SetCursorScreenPos( ImVec2( info.timeline.canvasPosX, info.timeline.canvasPosY ) );

   char threadNameBuffer[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread" );
   const float timelineOffsetY = info.timeline.canvasPosY + info.timeline.scrollAmount;
   for ( size_t i = 0; i < _tracks.size(); ++i )
   {
      // Skip empty threads
      if( _tracks[i].empty() ) continue;

      const bool threadHidden = _tracks[i].hidden();
      const float trackHeight = _tracks[i].heightWithThreadLabel();

      const char* threadName = &threadNameBuffer[0];
      if( _tracks[i].trackName() != 0 )
      {
         const size_t stringIdx = info.strDb.getStringIndex( _tracks[i].trackName() );
         threadName = info.strDb.getString( stringIdx );
      }
      else
      {
         snprintf(
             threadNameBuffer + threadNamePrefix, sizeof( threadNameBuffer ) - threadNamePrefix, "%lu", i );
      }
      HOP_PROF_DYN_NAME( threadName );

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
      drawLabels( labelsDrawPosition, threadName, _tracks, i, info );

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      _tracks[i]._absoluteDrawPos[0] = absDrawPos.x;
      _tracks[i]._absoluteDrawPos[1] = absDrawPos.y + info.timeline.scrollAmount - timelineOffsetY;
      _tracks[i]._localDrawPos[0] = absDrawPos.x;
      _tracks[i]._localDrawPos[1] = absDrawPos.y;

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
            drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            drawTraces( i, curDrawPos.x, curDrawPos.y, info, timelineActions );

            drawHighlightedTraces(
                _tracks[i]._highlightsDrawData,
                _highlightValue );
            _tracks[i]._highlightsDrawData.clear();

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
   }

   drawContextMenu( info );

   return timelineActions;
}

float TimelineTracks::totalHeight() const
{
   float height = 0.0f;
   const size_t trackCount = _tracks.size();
   if( trackCount != 0 )
   {
      height = _tracks[trackCount-1]._absoluteDrawPos[1] + _tracks[trackCount-1].height();
   }
   return height;
}

// Returns the index of the first set bit
static uint32_t setBitIndex( ZoneId_t zone )
{
   uint32_t count = 0;
   while ( zone )
   {
      zone = zone >> 1;
      ++count;
   }
   return count-1;
}

void TimelineTracks::drawTraces(
    uint32_t threadIndex,
    const float posX,
    const float posY,
    const TimelineTracksDrawInfo& drawInfo,
    std::vector< TimelineMessage >& timelineMsg )
{
   const TimelineTrack& data = _tracks[ threadIndex ];

   if ( data._traces.entries.ends.empty() ) return;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   const float windowWidthPxl = ImGui::GetWindowWidth();

   static std::array< DrawData, HOP_MAX_ZONE_COLORS > tracesToDraw;
   static std::array< DrawData, HOP_MAX_ZONE_COLORS > lodTracesToDraw;
   for( size_t i = 0; i < lodTracesToDraw.size(); ++i )
   {
      tracesToDraw[ i ].entries.clear();
      lodTracesToDraw[ i ].entries.clear();
      tracesToDraw[i].entryData.tData = lodTracesToDraw[i].entryData.tData = &data._traces;
   }

   // Find the best lodLevel for our current zoom
   const int lodLevel = _lodLevel;
   g_stats.currentLOD = lodLevel;

   // Get all the timing boundaries
   const TimeStamp globalStartTime = drawInfo.timeline.globalStartTime;
   const TimeStamp relativeStart = drawInfo.timeline.relativeStartTime;
   const TimeDuration timelineRange = drawInfo.timeline.duration;

   const TimeStamp absoluteStart = relativeStart + globalStartTime;
   const TimeStamp absoluteEnd = absoluteStart + timelineRange;

    // The time range to draw in absolute time
   const auto spanLodIndex =
       visibleIndexSpan( data._traces.lods, lodLevel, absoluteStart, absoluteEnd, 0 );

   if( spanLodIndex.first == hop::INVALID_IDX ) return;

   // Gather draw data for all visible traces
   HOP_PROF_SPLIT( "Creating draw data" );
   for ( size_t i = spanLodIndex.first; i < spanLodIndex.second; ++i )
   {
      const auto& t = data._traces.lods[lodLevel][i];
      const uint32_t zoneIndex = setBitIndex( data._traces.zones[t.traceIndex] );
      auto& lodToDraw = t.isLoded ? lodTracesToDraw : tracesToDraw;
      lodToDraw[zoneIndex].entries.push_back( createDrawDataForEntry(
             t.end, t.delta, t.depth, t.traceIndex, posX, posY, drawInfo, windowWidthPxl ) );
   }

   const bool drawAsCycles = drawInfo.timeline.useCycles;
   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;

   // Draw the loded traces
   HOP_PROF_SPLIT( "Drawing LOD traces" );
   char curName[512] = "<Multiple Elements> ~";
   const size_t hoveredNamePrefixSize = strlen( curName );
   for ( size_t zoneId = 0; zoneId < lodTracesToDraw.size(); ++zoneId )
   {
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[zoneId] );
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      const auto& traces = lodTracesToDraw[ zoneId ];
      for( const auto& t : traces.entries )
      {
         ImGui::SetCursorScreenPos( t.posPxl );

         ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );

         if ( ImGui::IsItemHovered() )
         {
            if ( t.lengthPxl > 3 )
            {
               ImGui::BeginTooltip();
               formatCyclesDurationToDisplay(
                   t.duration,
                   curName + hoveredNamePrefixSize,
                   sizeof( curName ) - hoveredNamePrefixSize,
                   drawAsCycles );
               ImGui::TextUnformatted( curName );
               ImGui::EndTooltip();
            }

            _tracks[threadIndex]._highlightsDrawData.emplace_back(
               TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

            if ( leftMouseDblClicked )
            {
               const TimeStamp traceEndTime =
                   pxlToCycles( windowWidthPxl, timelineRange, t.posPxl.x - posX + t.lengthPxl );

               TimelineMessage msg;
               msg.type = TimelineMessageType::FRAME_TO_TIME;
               msg.frameToTime.time = relativeStart + ( traceEndTime - t.duration );
               msg.frameToTime.duration = t.duration;
               msg.frameToTime.pushNavState = true;

               timelineMsg.push_back( msg );
            }
            else if ( rightMouseClicked && !drawInfo.timeline.mouseDragging )
            {
               ImGui::OpenPopup( CTXT_MENU_STR );
               _contextMenuInfo.open = true;
               _contextMenuInfo.traceClick = true;
               _contextMenuInfo.threadIndex = threadIndex;
               _contextMenuInfo.traceId = t.traceIndex;
            }
         }
      }
      ImGui::PopStyleColor(2);
      ImGui::PopStyleVar();
   }

   char formattedTime[64] = {};
   // Draw the non-loded traces
   HOP_PROF_SPLIT( "Drawing regular traces" );
   for ( size_t zoneId = 0; zoneId < tracesToDraw.size(); ++zoneId )
   {
      ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[zoneId] );
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      const auto& traces = tracesToDraw[ zoneId ];
      
      drawNonLodEntries( traces, drawInfo.strDb, nonLodTraceName, getHoveredTraceInformation );

      ImGui::PopStyleColor(2);
      ImGui::PopStyleVar();
   }

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.traceDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
}

std::vector< LockOwnerInfo > TimelineTracks::highlightLockOwner(
    uint32_t threadIndex,
    uint32_t hoveredLwIndex,
    const TimelineTracksDrawInfo& info )
{
    HOP_PROF_FUNC();
    std::vector< LockOwnerInfo > lockInfos;
    lockInfos.reserve( 16 );

    ImDrawList* DrawList = ImGui::GetOverlayDrawList();
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const int highlightAlpha = 70.0f * _highlightValue;

    // Info of the currently highlighted lock wait
    const void* highlightedMutexAddr = _tracks[threadIndex]._lockWaits.mutexAddrs[hoveredLwIndex];
    const TimeDuration highlightedLWDelta = _tracks[threadIndex]._lockWaits.entries.deltas[hoveredLwIndex];
    const TimeStamp highlightedLWEndTime = _tracks[threadIndex]._lockWaits.entries.ends[hoveredLwIndex];
    const TimeStamp highlightedLWStartTime = highlightedLWEndTime - highlightedLWDelta;

    // Go through all the threads and find the one that owned the lock during the
    // highlighted periods
    for (size_t i = 0; i < _tracks.size(); ++i)
    {
        if (i == threadIndex || _tracks[i].hidden() ) continue;

        const LockWaitData& lockWaits = _tracks[i]._lockWaits;

        const auto lastUnlock = std::lower_bound(
            _tracks[i]._lockWaits.entries.ends.cbegin(),
            _tracks[i]._lockWaits.entries.ends.cend(),
            highlightedLWEndTime );

        // This is the first lockdata that was acquired after the highlighted trace end
        auto lockDataIdx = std::distance( _tracks[i]._lockWaits.entries.ends.cbegin(), lastUnlock );
        if( lockDataIdx != 0 ) --lockDataIdx;

        while( lockDataIdx != 0 )
        {
            if( lockWaits.mutexAddrs[ lockDataIdx ] == highlightedMutexAddr )
            {
               const TimeStamp lockWaitEndTime = lockWaits.entries.ends[lockDataIdx];
               const TimeStamp unlockTime = lockWaits.lockReleases[lockDataIdx];

               // We've gone to far, so early break
               if( unlockTime != 0 && unlockTime < highlightedLWStartTime - 10000 )
                  break;

               const TimeDuration lockHoldDuration = unlockTime - lockWaitEndTime;

               // Add info to result vector
               bool added = false;
               for ( auto& info : lockInfos )
               {
                  if ( info.threadIndex == i )
                  {
                     info.lockDuration += lockHoldDuration;
                     added = true;
                     break;
                  }
               }
               if ( !added )
                  lockInfos.emplace_back( lockHoldDuration, i );

               const float tracesHeight = _tracks[i].heightWithThreadLabel();

               auto drawData = createDrawDataForEntry(
                  lockWaitEndTime,
                  lockHoldDuration,
                  0,
                  i,
                  _tracks[i]._localDrawPos[0],
                  _tracks[i]._localDrawPos[1],
                  info,
                  windowWidthPxl );

               ImVec2 topLeft = drawData.posPxl;
               topLeft.x += drawData.lengthPxl;
               ImVec2 bottomRight = topLeft;
               bottomRight.x += drawData.lengthPxl;
               bottomRight.y += tracesHeight;

               DrawList->AddRectFilled(
                   topLeft, bottomRight, ImColor( 0, 255, 0, 30 + highlightAlpha ) );
            }

            --lockDataIdx;
        }
    }

    return lockInfos;
}

void TimelineTracks::drawLockWaits(
    const uint32_t threadIndex,
    const float posX,
    const float posY,
    const TimelineTracksDrawInfo& drawInfo,
    std::vector< TimelineMessage >& timelineMsg )
{
   const LockWaitData& lockWaits =_tracks[ threadIndex ]._lockWaits;
   if ( lockWaits.entries.ends.empty() ) return;

   HOP_PROF_FUNC();
   const auto drawStart = std::chrono::system_clock::now();

   const auto absoluteStart = drawInfo.timeline.globalStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + drawInfo.timeline.relativeStartTime;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + drawInfo.timeline.duration;

   const int lodLevel = _lodLevel;

   const auto spanLodIndex =
       visibleIndexSpan( lockWaits.lods, lodLevel, firstTraceAbsoluteTime, lastTraceAbsoluteTime, 1 );

   if ( spanLodIndex.first == hop::INVALID_IDX ) return;

   static DrawData tracesToDraw, lodTracesToDraw;
   tracesToDraw.entries.clear();
   lodTracesToDraw.entries.clear();

   HOP_PROF_SPLIT( "Gathering lock waits drawing info" );

   for ( size_t i = spanLodIndex.first; i < spanLodIndex.second; ++i )
   {
      const auto& t = lockWaits.lods[lodLevel][i];
      auto& lodToDraw = t.isLoded ? lodTracesToDraw : tracesToDraw;
      lodToDraw.entries.push_back( createDrawDataForEntry(
             t.end, t.delta, t.depth, t.traceIndex, posX, posY, drawInfo, windowWidthPxl ) );
   }

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;
   ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[HOP_MAX_ZONE_COLORS] );
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[HOP_MAX_ZONE_COLORS] );
   ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[HOP_MAX_ZONE_COLORS] ? 1.0f : disabledZoneOpacity );

   const bool drawAsCycles = drawInfo.timeline.useCycles;

   HOP_PROF_SPLIT( "Drawing Lock Wait Lod" );
   for ( const auto& t : lodTracesToDraw.entries )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
      if ( ImGui::IsItemHovered() )
      {
         const auto lockInfo = highlightLockOwner( threadIndex, t.traceIndex, drawInfo );
         if ( t.lengthPxl > 3 )
         {
            char lockTooltip[256] = "Waiting lock for ~";
            ImGui::BeginTooltip();
            formatCyclesDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ),
                drawAsCycles );

            ImGui::TextUnformatted( lockTooltip );
            ImGui::EndTooltip();
         }

         _tracks[threadIndex]._highlightsDrawData.emplace_back(
             TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

         if ( ImGui::IsMouseDoubleClicked( 0 ) )
         {
            const TimeDuration delta = lockWaits.entries.deltas[t.traceIndex];
            TimelineMessage msg;
            msg.type = TimelineMessageType::FRAME_TO_ABSOLUTE_TIME;
            msg.frameToTime.time = lockWaits.entries.ends[t.traceIndex] - delta;
            msg.frameToTime.duration = delta;
            msg.frameToTime.pushNavState = true;
            timelineMsg.push_back( msg );
         }
      }
   }

   HOP_PROF_SPLIT( "drawing non-lod" );
   for ( const auto& t : tracesToDraw.entries )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "Waiting lock...", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
      if ( ImGui::IsItemHovered() )
      {
         const auto lockInfo = highlightLockOwner( threadIndex, t.traceIndex, drawInfo );
         if ( t.lengthPxl > 3 )
         {
            char lockTooltip[256] = "Waiting lock for ";
            ImGui::BeginTooltip();
            formatCyclesDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ),
                drawAsCycles );

            _tracks[threadIndex]._highlightsDrawData.emplace_back(
             TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

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
                  formatCyclesDurationToDisplay(
                      i.lockDuration,
                      formattedLockTime,
                      sizeof( formattedLockTime ),
                      drawAsCycles );
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
            const TimeDuration delta = lockWaits.entries.deltas[t.traceIndex];
            TimelineMessage msg;
            msg.type = TimelineMessageType::FRAME_TO_ABSOLUTE_TIME;
            msg.frameToTime.time = lockWaits.entries.ends[t.traceIndex] - delta;
            msg.frameToTime.duration = delta;
            msg.frameToTime.pushNavState = true;
            timelineMsg.push_back( msg );
         }
      }
   }

   ImGui::PopStyleColor(2);
   ImGui::PopStyleVar();

   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.lockwaitsDrawingTimeMs +=
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();
}

void TimelineTracks::drawSearchWindow(
    const TimelineTracksDrawInfo& di,
    std::vector<TimelineMessage>& timelineMsg )
{
   HOP_PROF_FUNC();

   const SearchSelection selection = drawSearchResult( _searchRes, di, *this );

   if ( selection.selectedTraceIdx != (size_t)-1 && selection.selectedThreadIdx != (uint32_t)-1 )
   {
      const auto& timelinetrack = _tracks[selection.selectedThreadIdx];
      const TimeStamp absEndTime = timelinetrack._traces.entries.ends[selection.selectedTraceIdx];
      const TimeStamp delta = timelinetrack._traces.entries.deltas[selection.selectedTraceIdx];
      const Depth_t depth = timelinetrack._traces.entries.depths[selection.selectedTraceIdx];

      // If the thread was hidden, display it so we can see the selected trace
      _tracks[selection.selectedThreadIdx].setTrackHeight( 9999.0f );

      const TimeStamp startTime = absEndTime - delta - di.timeline.globalStartTime;
      const float verticalPosPxl = timelinetrack._absoluteDrawPos[1] +
                                   ( depth * TimelineTrack::PADDED_TRACE_SIZE ) -
                                   ( 3 * TimelineTrack::PADDED_TRACE_SIZE );

      // Create the timeline messages ( frame horizontally and vertically )
      TimelineMessage msg[2];
      msg[0].type = TimelineMessageType::FRAME_TO_TIME;
      msg[0].frameToTime.time = startTime;
      msg[0].frameToTime.duration = delta;
      msg[0].frameToTime.pushNavState = true;
      msg[1].type = TimelineMessageType::MOVE_VERTICAL_POS_PXL;
      msg[1].verticalPos.posPxl = verticalPosPxl;

      timelineMsg.insert( timelineMsg.end(), std::begin( msg ), std::end( msg ) );
   }

   if ( selection.hoveredTraceIdx != (size_t)-1 && selection.hoveredThreadIdx != (uint32_t)-1 )
   {
      addTraceToHighlight( selection.hoveredTraceIdx, selection.hoveredThreadIdx, di );
   }
}

void TimelineTracks::drawTraceDetailsWindow( const TimelineTracksDrawInfo& info, std::vector< TimelineMessage >& timelineMsg )
{
   const TraceDetailDrawResult traceDetailRes =
       drawTraceDetails( _traceDetails, _tracks, info.strDb, info.timeline.useCycles );

   if( traceDetailRes.clicked )
   {
      Depth_t minDepth = std::numeric_limits< Depth_t >::max();
      TimeStamp minTime = std::numeric_limits< TimeStamp >::max();
      TimeStamp maxTime = std::numeric_limits< TimeStamp >::min();
      const auto& timelinetrack = _tracks[traceDetailRes.hoveredThreadIdx];
      for( size_t idx : traceDetailRes.hoveredTraceIds )
      {
         minDepth = std::min( timelinetrack._traces.entries.depths[ idx ], minDepth );
         const TimeStamp end = timelinetrack._traces.entries.ends[ idx ];
         maxTime = std::max( end, maxTime );
         minTime = std::min( end - timelinetrack._traces.entries.deltas[ idx ], minTime );
      }

      const float verticalPosPxl = timelinetrack._absoluteDrawPos[1] +
                                   ( minDepth * TimelineTrack::PADDED_TRACE_SIZE ) -
                                   ( 3 * TimelineTrack::PADDED_TRACE_SIZE );

      // Create the timeline messages ( frame horizontally and vertically )
      TimelineMessage msg[2];
      msg[0].type = TimelineMessageType::FRAME_TO_ABSOLUTE_TIME;
      msg[0].frameToTime.time = minTime;
      msg[0].frameToTime.duration = maxTime - minTime;
      msg[0].frameToTime.pushNavState = true;
      msg[1].type = TimelineMessageType::MOVE_VERTICAL_POS_PXL;
      msg[1].verticalPos.posPxl = verticalPosPxl;

      timelineMsg.insert( timelineMsg.end(), std::begin( msg ), std::end( msg ) );
   }

   for( const auto& t : traceDetailRes.hoveredTraceIds )
   {
      addTraceToHighlight( t, traceDetailRes.hoveredThreadIdx, info );
   }
}

void TimelineTracks::drawContextMenu( const TimelineTracksDrawInfo& info )
{
   // No trace were right clicked. Check for right click in canvas
   if( ImGui::IsMouseReleased( 1 ) && !_contextMenuInfo.open && !info.timeline.mouseDragging )
   {
      ImGui::OpenPopup( CTXT_MENU_STR );
      _contextMenuInfo.open = true;

      // Find out where the right click happened to figure out which track needs to be profiled
      const float mousePosY = ImGui::GetMousePos().y;
      int i = 1;
      for( ; i < (int)_tracks.size(); ++i)
      {
         if( _tracks[ i ]._localDrawPos[1] - THREAD_LABEL_HEIGHT > mousePosY )
         {
            break;
         }
      }

      // Get the index of the previous valid track
      int64_t prevValidTrack = i - 1;
      while( prevValidTrack > 0 && _tracks[ prevValidTrack ].empty() )
         --prevValidTrack;

      assert( prevValidTrack >= 0 );

      _contextMenuInfo.threadIndex = prevValidTrack;
   }

   if ( _contextMenuInfo.open )
   {
      ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
      ImGui::SetNextWindowBgAlpha( 0.8f );  // Transparent background
      if ( ImGui::BeginPopupContextItem( CTXT_MENU_STR ) )
      {
         if( _contextMenuInfo.traceClick )
         {
            if ( ImGui::Selectable( "Trace Stats" ) )
            {
               _traceStats = createTraceStats(
                   _tracks[_contextMenuInfo.threadIndex]._traces,
                   _contextMenuInfo.threadIndex,
                   _contextMenuInfo.traceId );
            }
            else if ( ImGui::Selectable( "Profile Stack" ) )
            {
               _traceDetails = createTraceDetails(
                   _tracks[_contextMenuInfo.threadIndex]._traces,
                   _contextMenuInfo.threadIndex,
                   _contextMenuInfo.traceId );
            }
         }
         else
         {
            if ( ImGui::Selectable( "Profile Track" ) )
            {
                displayModalWindow( "Computing total trace size...", MODAL_TYPE_NO_CLOSE );
                const uint32_t tIdx = _contextMenuInfo.threadIndex;
                std::thread t( [ this, tIdx, dispTrace = _tracks[tIdx]._traces.copy() ]() {
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
         memset( &_contextMenuInfo, 0, sizeof( _contextMenuInfo ) );
      }
      ImGui::PopStyleVar();
   }
}

void TimelineTracks::addTraceToHighlight( size_t traceId, uint32_t threadIndex, const TimelineTracksDrawInfo& drawInfo )
{
   // Gather draw data for visible highlighted traces
   const TimelineTrack& data = _tracks[ threadIndex ];
   const DrawData::Entry dd = createDrawDataForEntry(
       data._traces.entries.ends[traceId],
       data._traces.entries.deltas[traceId],
       data._traces.entries.depths[traceId],
       traceId,
       data._localDrawPos[0],
       data._localDrawPos[1],
       drawInfo,
       ImGui::GetWindowWidth() );

   _tracks[threadIndex]._highlightsDrawData.emplace_back(
       TimelineTrack::HighlightDrawInfo{dd.posPxl[0], dd.posPxl[1], dd.lengthPxl, 0xFFFFFF} );
}

void TimelineTracks::resizeAllTracksToFit()
{
   float visibleTrackCount = 0;
   for( auto& t : _tracks )
      if( !t.empty() ) ++visibleTrackCount;

   float timelineCanvasHeight = ImGui::GetIO().DisplaySize.y;

   const float totalTraceHeight = timelineCanvasHeight - visibleTrackCount * THREAD_LABEL_HEIGHT;
   const float heightPerTrack = totalTraceHeight / visibleTrackCount;

   // Autofit all track except the last one
   const size_t lastThread = _tracks.size() - 1;
   for( size_t i = 0; i < lastThread; ++i )
      _tracks[i].setTrackHeight( heightPerTrack );

   _tracks[lastThread].setTrackHeight( 9999.0f );
}

void TimelineTracks::setAllTracksCollapsed( bool collapsed )
{
   const size_t trackCount = _tracks.size();
   const float heightVal = collapsed ? -9999.0f : 9999.0f;
   for( size_t i = 0; i < trackCount; ++i )
      _tracks[i].setTrackHeight( heightVal );
}

const TimelineTrack& TimelineTracks::operator[]( size_t index ) const
{
   return _tracks[ index ];
}

TimelineTrack& TimelineTracks::operator[]( size_t index )
{
   return _tracks[ index ];
}

size_t TimelineTracks::size() const
{
   return _tracks.size();
}

void TimelineTracks::resize( size_t size )
{
   _tracks.resize( size );
}

void TimelineTracks::clear()
{
   _tracks.clear();
   clearSearchResult( _searchRes );
   clearTraceDetails( _traceDetails );
   clearTraceStats( _traceStats );
}

} // namespace hop