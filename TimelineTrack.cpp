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

static const char* CTXT_MENU_STR = "Context Menu";

static bool drawSeparator( uint32_t threadIndex, bool highlightSeparator )
{
   const float drawPosY = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;
   ImVec2 p1 = ImGui::GetWindowPos();
   p1.y += drawPosY;

   ImVec2 p2 = p1;
   p2.x += ImGui::GetWindowSize().x;
   p2.y += + 1;

   const bool hovered = std::abs( ImGui::GetMousePos().y - p1.y ) < 7.0f && threadIndex > 0;

   uint32_t color = ImGui::GetColorU32(ImGuiCol_Separator);
   if( hovered && highlightSeparator )
   {
      hop::setCursor( hop::CURSOR_SIZE_NS );
      color = 0xFFFFFFFF;
   }

   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddLine( p1, p2, color, 2.0f );

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

namespace hop
{

float TimelineTrack::TRACE_HEIGHT = 20.0f;
float TimelineTrack::TRACE_VERTICAL_PADDING = 2.0f;
float TimelineTrack::PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

void TimelineTrack::setTrackName( TStrPtr_t name ) noexcept
{
   _trackName = name;
}

TStrPtr_t TimelineTrack::trackName() const noexcept
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
   _lockWaits.append( lockWaits );
}

void TimelineTrack::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
   // If we did not get any lock events prior to the unlock events, simply ignore them
   if( _lockWaits.entries.ends.empty() ) return;

   HOP_PROF_FUNC();
   for( const auto& ue : unlockEvents )
   {
      const auto first = std::lower_bound( _lockWaits.entries.ends.begin(), _lockWaits.entries.ends.end(), ue.time );
      int64_t firstIdx = std::distance( _lockWaits.entries.ends.begin(), first );
      if( firstIdx != 0 ) --firstIdx;

      // Try to find the associated lockwait with a maximum lock time of 10 seconds. This is to
      // ensure we do not traverse the whole lockwaits array in the case where we would have skipped
      // logging a lock 
      for( int64_t i = firstIdx; i >= 0 && (ue.time - _lockWaits.entries.ends[ i ] < 10000000000); --i )
      {
         if( _lockWaits.mutexAddrs[ i ] == ue.mutexAddress &&
             _lockWaits.entries.ends[ i ] < ue.time  )
         {
            // In some cases, we can receive an orphan unlock events. In those case we must not
            // overwrite the previous value as they are unrelated. This can happen if we start
            // recording after the lock has been acquired or if we missed a lock message because
            // of insufficient memory
            if( _lockWaits.lockReleases[ i ] == 0 )
               _lockWaits.lockReleases[ i ] = ue.time;

            break;
         }
      }
   }
}

void TimelineTrack::addCoreEvents( const std::vector<CoreEvent>& coreEvents )
{
   _coreEvents.data.insert( _coreEvents.data.end(), coreEvents.begin(), coreEvents.end() );

   assert_is_sorted( _coreEvents.data.begin(), _coreEvents.data.end() );
}

TDepth_t TimelineTrack::maxDepth() const noexcept
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
   const size_t tracesCount = ti._traces.entries.ends.size();
   const size_t lockwaitsCount = ti._lockWaits.entries.ends.size();
   const size_t serializedSize =
       // Traces
       sizeof( size_t ) +                            // Traces count
       sizeof( hop::TDepth_t ) +                     // Max depth
       sizeof( hop::TimeStamp ) * tracesCount +      // ends
       sizeof( hop::TimeDuration ) * tracesCount +   // deltas
       sizeof( hop::TStrPtr_t ) * tracesCount * 2 +  // fileNameId and fctNameIds
       sizeof( hop::TLineNb_t ) * tracesCount +      // lineNbs
       sizeof( hop::TZoneId_t ) * tracesCount +      // zones
       sizeof( hop::TDepth_t ) * tracesCount +       // depths

       // Lock Waits
       sizeof( size_t ) +                            // LockWaits count
       sizeof( hop::TimeStamp ) * lockwaitsCount +   // ends
       sizeof( hop::TimeDuration ) * lockwaitsCount +// deltas
       sizeof( hop::TDepth_t ) * lockwaitsCount +    // depths
       sizeof( void* ) * lockwaitsCount +            // mutexAddrs
       sizeof( hop::TimeStamp ) * lockwaitsCount;    // lockReleases

   return serializedSize;
}

size_t serialize( const TimelineTrack& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    size_t i = 0;

    // Serialize Traces
    {
    // Traces count
    const size_t tracesCount = ti._traces.entries.ends.size();
    memcpy( &data[i], &tracesCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // Max depth
    memcpy( &data[i], &ti._traces.entries.maxDepth, sizeof( hop::TDepth_t ) );
    i += sizeof( hop::TDepth_t );

    //ends
    std::copy( ti._traces.entries.ends.begin(), ti._traces.entries.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy( ti._traces.entries.deltas.begin(), ti._traces.entries.deltas.end(), (hop::TimeDuration*)&data[i] );
    i += sizeof( hop::TimeDuration ) * tracesCount;

    // fileNameIds
    std::copy( ti._traces.fileNameIds.begin(), ti._traces.fileNameIds.end(), (hop::TStrPtr_t*)&data[i]);
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // fctNameIds
    std::copy( ti._traces.fctNameIds.begin(), ti._traces.fctNameIds.end(), (hop::TStrPtr_t*)&data[i]);
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // lineNbs
    std::copy( ti._traces.lineNbs.begin(), ti._traces.lineNbs.end(), (hop::TLineNb_t*) &data[i] );
    i += sizeof( hop::TLineNb_t ) * tracesCount;

    // zones
    std::copy( ti._traces.zones.begin(), ti._traces.zones.end(), (hop::TZoneId_t*) &data[i] );
    i += sizeof( hop::TZoneId_t ) * tracesCount;

    // depths
    std::copy( ti._traces.entries.depths.begin(), ti._traces.entries.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Serialize LockWaits
    {
    // LockWaits count
    const size_t lockwaitsCount = ti._lockWaits.entries.ends.size();
    memcpy( &data[i], &lockwaitsCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // ends
    std::copy( ti._lockWaits.entries.ends.begin(), ti._lockWaits.entries.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * lockwaitsCount;

    // deltas
    std::copy( ti._lockWaits.entries.deltas.begin(), ti._lockWaits.entries.deltas.end(), (hop::TimeDuration*)&data[i] );
    i += sizeof( hop::TimeDuration ) * lockwaitsCount;

    // depths
    std::copy( ti._lockWaits.entries.depths.begin(), ti._lockWaits.entries.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * lockwaitsCount;

    // mutexAddrs
    std::copy( ti._lockWaits.mutexAddrs.begin(), ti._lockWaits.mutexAddrs.end(), (void**)&data[i] );
    i += sizeof( void* ) * lockwaitsCount;

    // lockReleases
    std::copy( ti._lockWaits.lockReleases.begin(), ti._lockWaits.lockReleases.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * lockwaitsCount;
    }

    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, TimelineTrack& ti )
{
    size_t i = 0;

    // Deserializing Traces
    {
    const size_t tracesCount = *(size_t*)&data[i];
    i += sizeof( size_t );
    ti._traces.entries.maxDepth = *(hop::TDepth_t*)&data[i];
    i += sizeof( hop::TDepth_t );

    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + tracesCount, std::back_inserter(ti._traces.entries.ends));
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + tracesCount, std::back_inserter(ti._traces.entries.deltas));
    i += sizeof( hop::TimeDuration ) * tracesCount;

    // fileNameIds
    std::copy((hop::TStrPtr_t*)&data[i], ((hop::TStrPtr_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.fileNameIds));
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // fctNameIds
    std::copy((hop::TStrPtr_t*) &data[i], ((hop::TStrPtr_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.fctNameIds));
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // lineNbs
    std::copy((hop::TLineNb_t*)&data[i], ((hop::TLineNb_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.lineNbs));
    i += sizeof( hop::TLineNb_t ) * tracesCount;

    // zones
    std::copy((hop::TZoneId_t*)&data[i], ((hop::TZoneId_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.zones));
    i += sizeof( hop::TZoneId_t ) * tracesCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + tracesCount, std::back_inserter(ti._traces.entries.depths));
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Deserializing LockWaits
    {
    const size_t lockWaitsCount = *(size_t*)&data[i];
    i += sizeof( size_t );
    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.entries.ends));
    i += sizeof( hop::TimeStamp ) * lockWaitsCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.entries.deltas));
    i += sizeof( hop::TimeDuration ) * lockWaitsCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.entries.depths));
    i += sizeof( hop::TDepth_t ) * lockWaitsCount;

    // mutexAddrs
    std::copy((void**)&data[i], ((void**) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.mutexAddrs));
    i += sizeof( void* ) * lockWaitsCount;

    // lockReleases
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.lockReleases));
    i += sizeof( hop::TimeStamp ) * lockWaitsCount;
    }

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

std::vector< TimelineMessage > TimelineTracks::draw( const DrawInfo& info )
{
   std::vector< TimelineMessage > timelineActions;
   timelineActions.reserve( 4 );

   drawTraceDetailsWindow( info, timelineActions );
   drawSearchWindow( info, timelineActions );
   drawTraceStats( _traceStats, info.strDb );

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

      const auto& zoneColors = g_options.zoneColors;
      uint32_t threadLabelCol = zoneColors[ (i+1) % HOP_MAX_ZONES ];
      if( threadHidden )
         threadLabelCol = DISABLED_COLOR;

      // Draw thread label
      ImGui::PushID(i);
      ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
      if ( ImGui::Button( threadName, ImVec2( 0, THREAD_LABEL_HEIGHT ) ) )
      {
         _tracks[i].setTrackHeight( _tracks[i].hidden() ? 99999.0f : -99999.0f );
      }
      ImGui::PopStyleColor();
      ImGui::PopID();

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
   for( const auto& t : _tracks )
      height += t.heightWithThreadLabel();

   return height;
}

// Returns the index of the first set bit
static uint32_t setBitIndex( TZoneId_t zone )
{
   uint32_t count = 0;
   while ( zone )
   {
      zone = zone >> 1;
      ++count;
   }
   return count-1;
}

namespace
{
   struct DrawData
   {
      ImVec2 posPxl;
      TimeDuration duration;
      size_t traceIndex;
      float lengthPxl;
   };

   DrawData createDrawDataForTrace(
       TimeStamp traceEnd,
       TimeDuration traceDelta,
       TDepth_t traceDepth,
       size_t traceIdx,
       const float posX,
       const float posY,
       const TimelineTracks::DrawInfo& drawInfo,
       const float windowWidthPxl )
   {
      const TimeStamp traceEndTime = ( traceEnd - drawInfo.timeline.globalStartTime );
      const auto traceEndPxl = nanosToPxl<float>(
          windowWidthPxl,
          drawInfo.timeline.duration,
          traceEndTime - drawInfo.timeline.relativeStartTime );
      const float traceLengthPxl = std::max(
          MIN_TRACE_LENGTH_PXL,
          nanosToPxl<float>( windowWidthPxl, drawInfo.timeline.duration, traceDelta ) );

      const auto tracePos = ImVec2(
          posX + traceEndPxl - traceLengthPxl,
          posY + traceDepth * TimelineTrack::PADDED_TRACE_SIZE );

      return DrawData{tracePos, traceDelta, traceIdx, traceLengthPxl};
   }
} // anonymous namespace

void TimelineTracks::drawTraces(
    uint32_t threadIndex,
    const float posX,
    const float posY,
    const DrawInfo& drawInfo,
    std::vector< TimelineMessage >& timelineMsg )
{
   const TimelineTrack& data = _tracks[ threadIndex ];

   if ( data._traces.entries.ends.empty() ) return;

   HOP_PROF_FUNC();

   const float windowWidthPxl = ImGui::GetWindowWidth();

   static std::array< std::vector< DrawData >, HOP_MAX_ZONES > tracesToDraw;
   static std::array< std::vector< DrawData >, HOP_MAX_ZONES > lodTracesToDraw;
   for( size_t i = 0; i < lodTracesToDraw.size(); ++i )
   {
      tracesToDraw[ i ].clear();
      lodTracesToDraw[ i ].clear();
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
      lodToDraw[zoneIndex].push_back( createDrawDataForTrace(
             t.end, t.delta, t.depth, t.traceIndex, posX, posY, drawInfo, windowWidthPxl ) );
   }

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
      for( const auto& t : traces )
      {
         ImGui::SetCursorScreenPos( t.posPxl );

         ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );

         if ( ImGui::IsItemHovered() )
         {
            if ( t.lengthPxl > 3 )
            {
               ImGui::BeginTooltip();
               formatNanosDurationToDisplay( t.duration, curName + hoveredNamePrefixSize, sizeof( curName ) - hoveredNamePrefixSize );
               ImGui::TextUnformatted( curName );
               ImGui::EndTooltip();
            }

            _tracks[threadIndex]._highlightsDrawData.emplace_back(
               TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

            if ( leftMouseDblClicked )
            {
               const TimeStamp traceEndTime =
                   pxlToNanos( windowWidthPxl, timelineRange, t.posPxl.x - posX + t.lengthPxl );

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
      for( const auto& t : traces )
      {
         const size_t traceIndex = t.traceIndex;
         snprintf( curName, sizeof(curName), "%s", drawInfo.strDb.getString( data._traces.fctNameIds[traceIndex] ) );

         ImGui::SetCursorScreenPos( t.posPxl );
         ImGui::Button( curName, ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
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
                   drawInfo.strDb.getString( data._traces.fileNameIds[traceIndex] ),
                   data._traces.lineNbs[traceIndex] );
               ImGui::TextUnformatted( curName );
               ImGui::EndTooltip();
            }

            // Highlight hovered trace
            _tracks[threadIndex]._highlightsDrawData.emplace_back(
               TimelineTrack::HighlightDrawInfo{t.posPxl[0], t.posPxl[1], t.lengthPxl, 0xFFFFFF} );

            if ( leftMouseDblClicked )
            {
               const TimeStamp startTime = data._traces.entries.ends[traceIndex] -
                                           data._traces.entries.deltas[traceIndex] - globalStartTime;
               TimelineMessage msg;
               msg.type = TimelineMessageType::FRAME_TO_TIME;
               msg.frameToTime.time = startTime;
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
}

std::vector< LockOwnerInfo > TimelineTracks::highlightLockOwner(
    uint32_t threadIndex,
    uint32_t hoveredLwIndex,
    const DrawInfo& info )
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
               if( unlockTime != 0 && unlockTime < highlightedLWStartTime )
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

               auto drawData = createDrawDataForTrace(
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
    const DrawInfo& drawInfo,
    std::vector< TimelineMessage >& timelineMsg )
{
   const LockWaitData& lockWaits =_tracks[ threadIndex ]._lockWaits;
   if ( lockWaits.entries.ends.empty() ) return;

   HOP_PROF_FUNC();

   const auto absoluteStart = drawInfo.timeline.globalStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + drawInfo.timeline.relativeStartTime;
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + drawInfo.timeline.duration;

   const int lodLevel = _lodLevel;

   const auto spanLodIndex =
       visibleIndexSpan( lockWaits.lods, lodLevel, firstTraceAbsoluteTime, lastTraceAbsoluteTime, 1 );

   if ( spanLodIndex.first == hop::INVALID_IDX ) return;

   static std::vector<DrawData> tracesToDraw, lodTracesToDraw;
   tracesToDraw.clear();
   lodTracesToDraw.clear();

   HOP_PROF_SPLIT( "Gathering lock waits drawing info" );

   for ( size_t i = spanLodIndex.first; i < spanLodIndex.second; ++i )
   {
      const auto& t = lockWaits.lods[lodLevel][i];
      auto& lodToDraw = t.isLoded ? lodTracesToDraw : tracesToDraw;
      lodToDraw.push_back( createDrawDataForTrace(
             t.end, t.delta, t.depth, t.traceIndex, posX, posY, drawInfo, windowWidthPxl ) );
   }

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;
   ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[HOP_MAX_ZONES] );
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, zoneColors[HOP_MAX_ZONES] );
   ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[HOP_MAX_ZONES] ? 1.0f : disabledZoneOpacity );

   HOP_PROF_SPLIT( "Drawing Lock Wait Lod" );
   for ( const auto& t : lodTracesToDraw )
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
            formatNanosDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ) );

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
   for ( const auto& t : tracesToDraw )
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
            formatNanosDurationToDisplay(
                t.duration,
                lockTooltip + strlen( lockTooltip ),
                sizeof( lockTooltip ) - strlen( lockTooltip ) );

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
                  formatNanosDurationToDisplay(
                      i.lockDuration, formattedLockTime, sizeof( formattedLockTime ) );
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
}

void TimelineTracks::drawSearchWindow(
    const DrawInfo& di,
    std::vector<TimelineMessage>& timelineMsg )
{
   HOP_PROF_FUNC();

   const SearchSelection selection = drawSearchResult(
       _searchRes, di.timeline.globalStartTime, di.timeline.duration, di.strDb, *this );

   if ( selection.selectedTraceIdx != (size_t)-1 && selection.selectedThreadIdx != (uint32_t)-1 )
   {
      const auto& timelinetrack = _tracks[selection.selectedThreadIdx];
      const TimeStamp absEndTime = timelinetrack._traces.entries.ends[selection.selectedTraceIdx];
      const TimeStamp delta = timelinetrack._traces.entries.deltas[selection.selectedTraceIdx];
      const TDepth_t depth = timelinetrack._traces.entries.depths[selection.selectedTraceIdx];

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

void TimelineTracks::drawTraceDetailsWindow( const DrawInfo& info, std::vector< TimelineMessage >& timelineMsg )
{
   const TraceDetailDrawResult traceDetailRes = drawTraceDetails( _traceDetails, _tracks, info.strDb );

   if( traceDetailRes.clicked )
   {
      TDepth_t minDepth = std::numeric_limits< TDepth_t >::max();
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

void TimelineTracks::drawContextMenu( const DrawInfo& info )
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
      while( i >= 0 && _tracks[ prevValidTrack ].empty() )
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
            else if ( ImGui::Selectable( "Resize Tracks to Fit" ) )
            {
               resizeAllTracksToFit();
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

void TimelineTracks::addTraceToHighlight( size_t traceId, uint32_t threadIndex, const DrawInfo& drawInfo )
{
   // Gather draw data for visible highlighted traces
   const TimelineTrack& data = _tracks[ threadIndex ];
   const DrawData dd = createDrawDataForTrace(
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

   float timelineCanvasHeight = ImGui::GetIO().DisplaySize.y;// - TIMELINE_TOTAL_HEIGHT;

   const float totalTraceHeight = timelineCanvasHeight - visibleTrackCount * THREAD_LABEL_HEIGHT;
   const float heightPerTrack = totalTraceHeight / visibleTrackCount;

   // Autofit all track except the last one
   const size_t lastThread = _tracks.size() - 1;
   for( size_t i = 0; i < lastThread; ++i )
      _tracks[i].setTrackHeight( heightPerTrack );

   _tracks[lastThread].setTrackHeight( 9999.0f );
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