#include "TimelineTrack.h"
#include "Lod.h"
#include "Utils.h"
#include "Options.h"
#include "Cursor.h"
#include "Stats.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

static constexpr float THREAD_LABEL_HEIGHT = 20.0f;
static constexpr float MIN_TRACE_LENGTH_PXL = 1.0f;
static constexpr float MAX_TRACE_HEIGHT = 50.0f;
static constexpr float MIN_TRACE_HEIGHT = 15.0f;
static constexpr uint32_t DISABLED_COLOR = 0xFF505050;
static constexpr uint32_t HOVERED_COLOR_DELTA = 0x00191919;
static constexpr uint32_t ACTIVE_COLOR_DELTA = 0x00333333;

namespace hop
{

float TimelineTrack::TRACE_HEIGHT = 20.0f;
float TimelineTrack::TRACE_VERTICAL_PADDING = 2.0f;
float TimelineTrack::PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

void TimelineTrack::addTraces( const TraceData& newTraces )
{
   _traces.append( newTraces );

   assert_is_sorted(_traces.ends.begin(), _traces.ends.end() );
}

void TimelineTrack::addLockWaits( const LockWaitData& lockWaits )
{
   _lockWaits.append( lockWaits );
}

void TimelineTrack::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

TDepth_t TimelineTrack::maxDepth() const noexcept
{
   return _traces.maxDepth;
}

float TimelineTrack::maxDisplayedDepth() const noexcept
{
   return std::min( (float)_traces.maxDepth, _trackHeight ) + 1.0f;
}

void TimelineTrack::setTrackHeight( float height )
{
   _trackHeight = hop::clamp( height, -1.0f, (float)maxDepth() );
}

bool TimelineTrack::empty() const
{
   return _traces.ends.empty();
}

size_t serializedSize( const TimelineTrack& ti )
{
   const size_t tracesCount = ti._traces.ends.size();
   const size_t lockwaitsCount = ti._lockWaits.ends.size();
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
       sizeof( void* ) * lockwaitsCount;             // mutexAddrs

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
    const size_t tracesCount = ti._traces.ends.size();
    memcpy( &data[i], &tracesCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // Max depth
    memcpy( &data[i], &ti._traces.maxDepth, sizeof( hop::TDepth_t ) );
    i += sizeof( hop::TDepth_t );

    //ends
    std::copy(ti._traces.ends.begin(), ti._traces.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy( ti._traces.deltas.begin(), ti._traces.deltas.end(), (hop::TimeDuration*)&data[i] );
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
    std::copy( ti._traces.depths.begin(), ti._traces.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Serialize LockWaits
    {
    // LockWaits count
    const size_t lockwaitsCount = ti._lockWaits.ends.size();
    memcpy( &data[i], &lockwaitsCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // ends
    std::copy(ti._lockWaits.ends.begin(), ti._lockWaits.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * lockwaitsCount;

    // deltas
    std::copy( ti._lockWaits.deltas.begin(), ti._lockWaits.deltas.end(), (hop::TimeDuration*)&data[i] );
    i += sizeof( hop::TimeDuration ) * lockwaitsCount;

    // depths
    std::copy( ti._lockWaits.depths.begin(), ti._lockWaits.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * lockwaitsCount;

    // mutexAddrs
    std::copy( ti._lockWaits.mutexAddrs.begin(), ti._lockWaits.mutexAddrs.end(), (void**)&data[i] );
    i += sizeof( void* ) * lockwaitsCount;
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
    ti._traces.maxDepth = *(hop::TDepth_t*)&data[i];
    i += sizeof( hop::TDepth_t );

    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + tracesCount, std::back_inserter(ti._traces.ends));
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + tracesCount, std::back_inserter(ti._traces.deltas));
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
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + tracesCount, std::back_inserter(ti._traces.depths));
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Deserializing LockWaits
    {
    const size_t lockWaitsCount = *(size_t*)&data[i];
    i += sizeof( size_t );
    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.ends));
    i += sizeof( hop::TimeStamp ) * lockWaitsCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.deltas));
    i += sizeof( hop::TimeDuration ) * lockWaitsCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.depths));
    i += sizeof( hop::TDepth_t ) * lockWaitsCount;

    // mutexAddrs
    std::copy((void**)&data[i], ((void**) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.mutexAddrs));
    i += sizeof( void* ) * lockWaitsCount;
    }

    return i;
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

   uint32_t color = ImGui::GetColorU32(ImGuiCol_Separator);
   if( hovered && highlightSeparator )
   {
      hop::setCursor( hop::CURSOR_SIZE_NS );
      color = 0xFFFFFFFF;
   }

   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddLine( p1, p2, color, 2.0f );

   ImGui::SetCursorPosY( ImGui::GetCursorPosY() );

   return hovered;
}

void TimelineTracks::update( TimeDuration timelineDuration )
{
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

void TimelineTracks::draw( const DrawInfo& info )
{
   char threadName[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread" );
   for ( size_t i = 0; i < _tracks.size(); ++i )
   {
      // Skip empty threads
      if( _tracks[i].empty() ) continue;

      const bool threadHidden = _tracks[i].maxDisplayedDepth() <= 0.0f;
      const float trackHeight = _tracks[i].maxDisplayedDepth() * TimelineTrack::PADDED_TRACE_SIZE;
      snprintf(
          threadName + threadNamePrefix, sizeof( threadName ) - threadNamePrefix, "%lu", i );
      HOP_PROF_DYN_NAME( threadName );

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const auto& zoneColors = g_options.zoneColors;
      uint32_t threadLabelCol = zoneColors[ (i+1) % HOP_MAX_ZONES ];
      if( threadHidden )
         threadLabelCol = DISABLED_COLOR;

      ImGui::PushID(i);
      ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, addColorWithClamping( threadLabelCol, HOVERED_COLOR_DELTA ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, addColorWithClamping( threadLabelCol, ACTIVE_COLOR_DELTA ) );
      if ( ImGui::Button( threadName, ImVec2( 0, THREAD_LABEL_HEIGHT ) ) )
      {
         _tracks[i]._trackHeight = threadHidden ? 9999.0f : -1.0f;
      }

      ImGui::PopStyleColor( 3 );
      ImGui::PopID();

      // Then draw the interesting stuff
      _tracks[i]._localTracesVerticalStartPos = ImGui::GetCursorPosY();
      const float absTracesVerticalStartPos = ImGui::GetCursorScreenPos().y;
      _tracks[i]._absoluteTracesVerticalStartPos = absTracesVerticalStartPos;

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

      ImVec2 curDrawPos = ImGui::GetCursorScreenPos();
      if (!threadHidden)
      {
         const float threadStartRelDrawPos = curDrawPos.y - ImGui::GetWindowPos().y;
         const float threadEndRelDrawPos = threadStartRelDrawPos + trackHeight;

         const bool tracesVisible =
             !( threadStartRelDrawPos > ImGui::GetWindowHeight() || threadEndRelDrawPos < 0 );

         if( tracesVisible )
         {
            ImGui::PushClipRect(
                ImVec2( 0.0f, curDrawPos.y ),
                ImVec2( 9999.0f, curDrawPos.y + trackHeight ),
                true );

            // Draw the lock waits (before traces so that they are not hiding them)
            //drawLockWaits( _tracks, i, drawPosX, absTracesVerticalStartPos );
            drawTraces( _tracks[i], i, curDrawPos.x, curDrawPos.y, info );

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
   }
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

void TimelineTracks::drawTraces(
    const TimelineTrack& data,
    uint32_t threadIndex,
    const float posX,
    const float posY,
    const DrawInfo& drawInfo )
{
   if ( data._traces.ends.empty() ) return;

   HOP_PROF_FUNC();

   const float windowWidthPxl = ImGui::GetWindowWidth();

   struct DrawData
   {
      ImVec2 posPxl;
      TimeDuration duration;
      size_t traceIndex;
      float lengthPxl;
   };

   static std::array< std::vector< DrawData >, HOP_MAX_ZONES > tracesToDraw;
   static std::array< std::vector< DrawData >, HOP_MAX_ZONES > lodTracesToDraw;
   for( size_t i = 0; i < lodTracesToDraw.size(); ++i )
   {
      tracesToDraw[ i ].clear();
      lodTracesToDraw[ i ].clear();
   }

   static std::vector<DrawData> highlightTraceToDraw;
   highlightTraceToDraw.clear();

   // Find the best lodLevel for our current zoom
   const int lodLevel = _lodLevel;
   g_stats.currentLOD = lodLevel;

   // The time range to draw in absolute time
   const TimeStamp relativeStart = drawInfo.timelineRelativeStartTime;
   const TimeStamp absoluteStart = drawInfo.timelineAbsoluteStartTime;
   const TimeStamp absoluteEnd = drawInfo.timelineAbsoluteEndTime;
   const TimeDuration timelineRange = absoluteEnd - absoluteStart;

   const auto span =
       visibleIndexSpan( data._traces.lods, lodLevel, absoluteStart, absoluteEnd, 0 );

   if( span.first == hop::INVALID_IDX ) return;

   for ( size_t i = span.first; i < span.second; ++i )
   {
      const auto& t = data._traces.lods[lodLevel][i];
      const TimeStamp traceEndTime = ( t.end - absoluteStart );
      const auto traceEndPxl = nanosToPxl<float>(
          windowWidthPxl, timelineRange, traceEndTime - relativeStart );
      const float traceLengthPxl = std::max(
          MIN_TRACE_LENGTH_PXL, nanosToPxl<float>( windowWidthPxl, timelineRange, t.delta ) );

      const auto tracePos = ImVec2(
          posX + traceEndPxl - traceLengthPxl,
          posY + t.depth * TimelineTrack::PADDED_TRACE_SIZE);
      const uint32_t zoneIndex = setBitIndex(data._traces.zones[t.traceIndex]);
      if ( t.isLoded )
      {
         lodTracesToDraw[ zoneIndex ].push_back(
             DrawData{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
      }
      else
      {
         tracesToDraw[ zoneIndex ].push_back(
             DrawData{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
         for( const auto& tid : _highlightedTraces )
         {
            if( threadIndex == tid.second && t.traceIndex == tid.first )
            {
               highlightTraceToDraw.push_back( DrawData{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
            }
         }
      }
   }

   const bool rightMouseClicked = ImGui::IsMouseReleased( 1 );
   const bool leftMouseDblClicked = ImGui::IsMouseDoubleClicked( 0 );

   const auto& zoneColors = g_options.zoneColors;
   const auto& enabledZone = g_options.zoneEnabled;
   const float disabledZoneOpacity = g_options.disabledZoneOpacity;

   // Draw the loded traces
   char curName[512] = "<Multiple Elements> ~";
   const size_t hoveredNamePrefixSize = strlen( curName );
   for ( size_t zoneId = 0; zoneId < lodTracesToDraw.size(); ++zoneId )
   {
      // ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[zoneId], HOVERED_COLOR_DELTA ) );
      // ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[zoneId], ACTIVE_COLOR_DELTA ) );
      // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      // const auto& traces = lodTracesToDraw[ zoneId ];
      // for( const auto& t : traces )
      // {
      //    ImGui::SetCursorScreenPos( t.posPxl );

      //    ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );

      //    if ( ImGui::IsItemHovered() )
      //    {
      //       if ( t.lengthPxl > 3 )
      //       {
      //          ImGui::BeginTooltip();
      //          formatNanosDurationToDisplay( t.duration, curName + hoveredNamePrefixSize, sizeof( curName ) - hoveredNamePrefixSize );
      //          ImGui::TextUnformatted( curName );
      //          ImGui::EndTooltip();
      //       }

      //       if ( leftMouseDblClicked )
      //       {
      //          pushNavigationState();
      //          const TimeStamp traceEndTime =
      //              pxlToNanos( windowWidthPxl, _timelineRange, t.posPxl.x - posX + t.lengthPxl );
      //          frameToTime( _timelineStart + ( traceEndTime - t.duration ), t.duration );
      //       }
      //       else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f)
      //       {
      //          ImGui::OpenPopup( "Context Menu" );
      //          _contextMenuInfo.open = true;
      //          _contextMenuInfo.threadIndex = threadIndex;
      //          _contextMenuInfo.traceId = t.traceIndex;
      //       }
      //    }
      // }
      // ImGui::PopStyleColor( 3 );
      // ImGui::PopStyleVar();
   }

   char formattedTime[64] = {};
   // Draw the non-loded traces
   for ( size_t zoneId = 0; zoneId < tracesToDraw.size(); ++zoneId )
   {
      // ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[zoneId] );
      // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[zoneId], HOVERED_COLOR_DELTA ) );
      // ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[zoneId], ACTIVE_COLOR_DELTA ) );
      // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[zoneId] ? 1.0f : disabledZoneOpacity );
      // const auto& traces = tracesToDraw[ zoneId ];
      // for( const auto& t : traces )
      // {
      //    const size_t traceIndex = t.traceIndex;
      //    snprintf( curName, sizeof(curName), "%s", strDb.getString( data._traces.fctNameIds[traceIndex] ) );

      //    ImGui::SetCursorScreenPos( t.posPxl );
      //    ImGui::Button( curName, ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
      //    if ( ImGui::IsItemHovered() )
      //    {
      //       if ( t.lengthPxl > 3 )
      //       {
      //          size_t lastChar = strlen( curName );
      //          curName[lastChar] = ' ';
      //          ImGui::BeginTooltip();
      //          formatNanosDurationToDisplay( t.duration, formattedTime, sizeof( formattedTime ) );
      //          snprintf(
      //              curName + lastChar,
      //              sizeof( curName ) - lastChar,
      //              " (%s)\n   %s:%d ",
      //              formattedTime,
      //              strDb.getString( data._traces.fileNameIds[traceIndex] ),
      //              data._traces.lineNbs[traceIndex] );
      //          ImGui::TextUnformatted( curName );
      //          ImGui::EndTooltip();
      //       }

      //       if ( leftMouseDblClicked )
      //       {
      //          setZoom( t.duration );
      //          setStartTime(
      //              ( data._traces.ends[traceIndex] - data._traces.deltas[traceIndex] - absoluteStart ) );
      //       }
      //       else if ( rightMouseClicked && _rightClickStartPosInCanvas[0] == 0.0f )
      //       {
      //          ImGui::OpenPopup( "Context Menu" );
      //          _contextMenuInfo.open = true;
      //          _contextMenuInfo.threadIndex = threadIndex;
      //          _contextMenuInfo.traceId = t.traceIndex;
      //       }
      //    }
      // }
      // ImGui::PopStyleColor( 3 );
      // ImGui::PopStyleVar();
   }

   ImGui::PushStyleColor( ImGuiCol_Button, ImColor( 1.0f, 1.0f, 1.0f, 0.5f * drawInfo.hightlighPct ).Value );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * drawInfo.hightlighPct ).Value );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( 1.0f, 1.0f, 1.0f, 0.4f * drawInfo.hightlighPct ).Value );
   for( const auto& t : highlightTraceToDraw )
   {
      ImGui::SetCursorScreenPos( t.posPxl );
      ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
   }
   ImGui::PopStyleColor( 3 );
}

// std::vector< Timeline::LockOwnerInfo > Timeline::highlightLockOwner(
//     const TimelineTracks& infos,
//     uint32_t threadIndex,
//     uint32_t hoveredLwIndex,
//     const float posX,
//     const float /*posY*/ )
// {
//     HOP_PROF_FUNC();
//     std::vector< LockOwnerInfo > lockInfos;
//     lockInfos.reserve( 16 );

//     ImDrawList* DrawList = ImGui::GetWindowDrawList();
//     const float windowWidthPxl = ImGui::GetWindowWidth();
//     const auto absoluteStart = _absoluteStartTime;
//     const int highlightAlpha = 70.0f * _animationState.highlightPercent;

//     const void* highlightedMutexAddr = infos[threadIndex]._lockWaits.mutexAddrs[hoveredLwIndex];
//     const TimeDuration highlightedLWDelta = infos[threadIndex]._lockWaits.deltas[hoveredLwIndex];
//     const TimeStamp highlightedLWEndTime = infos[threadIndex]._lockWaits.ends[hoveredLwIndex];
//     const TimeStamp highlightedLWStartTime = highlightedLWEndTime - highlightedLWDelta;
//     for (size_t i = 0; i < infos.size(); ++i)
//     {
//         if (i == threadIndex || infos[i].maxDisplayedDepth() <= 0.0f ) continue;

//         const LockWaitData& lockWaits = infos[i]._lockWaits;

//         const float startNanosAsPxl =
//            nanosToPxl<float>(windowWidthPxl, _timelineRange, _timelineStart);

//         auto lastUnlock = std::lower_bound(
//             infos[i]._unlockEvents.cbegin(),
//             infos[i]._unlockEvents.cend(),
//             highlightedLWEndTime,
//             unlock_events_less_cmp() );

//         // lower_bound returns the first that is not smaller. We need the one just before that
//         if(lastUnlock != infos[i]._unlockEvents.cbegin() ) --lastUnlock;

//         while( lastUnlock != infos[i]._unlockEvents.cbegin() )
//         {
//             if(lastUnlock->mutexAddress == highlightedMutexAddr )
//             {
//                // We've gone to far, so early break
//                if(lastUnlock->time < highlightedLWStartTime )
//                   break;

//                // Find the associated lock wait
//                const auto lockWaitIt = std::lower_bound(
//                    lockWaits.ends.cbegin(), lockWaits.ends.cend(), lastUnlock->time );
//                size_t lockWaitIdx = std::distance( lockWaits.ends.begin(), lockWaitIt );

//                // lower_bound returns the first that does not compare smaller than the unlock time.
//                // Therefore, we need to start from this iterator and find the first one that matches
//                // the highlighted mutex
//                if( lockWaitIdx > 0 ) --lockWaitIdx;

//                while ( lockWaitIdx > 0 &&
//                        lockWaits.mutexAddrs[lockWaitIdx] != highlightedMutexAddr )
//                {
//                   --lockWaitIdx;
//                }

//                const TimeStamp lockWaitEndTime = lockWaits.ends[lockWaitIdx];
//                // Add info to result vector
//                bool added = false;
//                for( auto& info : lockInfos )
//                {
//                   if( info.threadIndex == i )
//                   {
//                      info.lockDuration += lastUnlock->time - lockWaitEndTime;
//                      added = true;
//                      break;
//                   }
//                }
//                if( !added )
//                   lockInfos.emplace_back( lastUnlock->time - lockWaitEndTime, i );

//                const int64_t lockTimeAsPxl = nanosToPxl<float>(
//                   windowWidthPxl,
//                   _timelineRange,
//                   (lockWaitEndTime - absoluteStart));
//                const int64_t unlockTimeAsPxl = nanosToPxl<float>(
//                   windowWidthPxl, _timelineRange, (lastUnlock->time - absoluteStart));

//                const float tracesHeight = infos[i].maxDisplayedDepth() * TimelineTrack::PADDED_TRACE_SIZE;

//                DrawList->AddRectFilled(
//                   ImVec2(posX - startNanosAsPxl + lockTimeAsPxl, infos[i]._absoluteTracesVerticalStartPos),
//                   ImVec2(
//                      posX - startNanosAsPxl + unlockTimeAsPxl,
//                      infos[i]._absoluteTracesVerticalStartPos + tracesHeight),
//                   ImColor(0, 255, 0, 30 + highlightAlpha));
//             }

//             --lastUnlock;
//         }
//     }

//     return lockInfos;
// }

void TimelineTracks::drawLockWaits(
    uint32_t threadIndex,
    const float posX,
    const float posY )
{
   // const auto& data = infos[threadIndex];
   // const LockWaitData& lockWaits = data._lockWaits;
   // if ( lockWaits.ends.empty() ) return;

   // HOP_PROF_FUNC();

   // const auto absoluteStart = _absoluteStartTime;
   // const float windowWidthPxl = ImGui::GetWindowWidth();

   // // The time range to draw in absolute time
   // const TimeStamp firstTraceAbsoluteTime = absoluteStart + _timelineStart;
   // const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + _timelineRange;

   // // Find the best lodLevel for our current zoom
   // const int lodLevel = currentLodLevel();

   // struct DrawingInfo
   // {
   //    ImVec2 posPxl;
   //    TimeDuration duration;
   //    size_t traceIndex;
   //    float lengthPxl;
   // };

   // const auto span =
   //     visibleIndexSpan( lockWaits.lods, lodLevel, firstTraceAbsoluteTime, lastTraceAbsoluteTime, 1 );

   // if ( span.first == hop::INVALID_IDX ) return;

   // static std::vector<DrawingInfo> tracesToDraw, lodTracesToDraw;
   // tracesToDraw.clear();
   // lodTracesToDraw.clear();

   // HOP_PROF_SPLIT( "Gathering drawing info" );

   // for ( size_t i = span.first; i < span.second; ++i )
   // {
   //    const auto& t = lockWaits.lods[lodLevel][i];
   //    const TimeStamp traceEndTime = ( t.end - absoluteStart );
   //    const auto traceEndPxl =
   //        nanosToPxl<float>( windowWidthPxl, _timelineRange, traceEndTime - _timelineStart );
   //    const float traceLengthPxl = std::max(
   //        MIN_TRACE_LENGTH_PXL, nanosToPxl<float>( windowWidthPxl, _timelineRange, t.delta ) );

   //    const auto tracePos =
   //        ImVec2( posX + traceEndPxl - traceLengthPxl, posY + t.depth * TimelineTrack::PADDED_TRACE_SIZE );
   //    if ( t.isLoded )
   //    {
   //       lodTracesToDraw.push_back( DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
   //    }
   //    else
   //    {
   //       tracesToDraw.push_back( DrawingInfo{tracePos, t.delta, t.traceIndex, traceLengthPxl} );
   //    }
   // }

   // const auto& zoneColors = g_options.zoneColors;
   // const auto& enabledZone = g_options.zoneEnabled;
   // const float disabledZoneOpacity = g_options.disabledZoneOpacity;
   // ImGui::PushStyleColor(ImGuiCol_Button, zoneColors[HOP_MAX_ZONES] );
   // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, addColorWithClamping( zoneColors[HOP_MAX_ZONES], HOVERED_COLOR_DELTA ) );
   // ImGui::PushStyleColor(ImGuiCol_ButtonActive, addColorWithClamping( zoneColors[HOP_MAX_ZONES], ACTIVE_COLOR_DELTA ) );
   // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, enabledZone[HOP_MAX_ZONES] ? 1.0f : disabledZoneOpacity );

   // HOP_PROF_SPLIT( "drawing lod" );
   // for ( const auto& t : lodTracesToDraw )
   // {
   //    ImGui::SetCursorScreenPos( t.posPxl );
   //    ImGui::Button( "", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
   //    if ( ImGui::IsItemHovered() )
   //    {
   //       const auto lockInfo = highlightLockOwner( infos, threadIndex, t.traceIndex, posX, posY );
   //       (void)lockInfo.empty();
   //       if ( t.lengthPxl > 3 )
   //       {
   //          char lockTooltip[256] = "Waiting lock for ~";
   //          ImGui::BeginTooltip();
   //          formatNanosDurationToDisplay(
   //              t.duration,
   //              lockTooltip + strlen( lockTooltip ),
   //              sizeof( lockTooltip ) - strlen( lockTooltip ) );

   //          ImGui::TextUnformatted( lockTooltip );
   //          ImGui::EndTooltip();
   //       }

   //       if ( ImGui::IsMouseDoubleClicked( 0 ) )
   //       {
   //          pushNavigationState();
   //          const TimeDuration delta = lockWaits.deltas[t.traceIndex];
   //          frameToAbsoluteTime( lockWaits.ends[t.traceIndex] - delta, delta );
   //       }
   //    }
   // }

   // HOP_PROF_SPLIT( "drawing non-lod" );
   // for ( const auto& t : tracesToDraw )
   // {
   //    ImGui::SetCursorScreenPos( t.posPxl );
   //    ImGui::Button( "Waiting lock...", ImVec2( t.lengthPxl, TimelineTrack::TRACE_HEIGHT ) );
   //    if ( ImGui::IsItemHovered() )
   //    {
   //       const auto lockInfo = highlightLockOwner( infos, threadIndex, t.traceIndex, posX, posY );
   //       (void)lockInfo.empty();
   //       if ( t.lengthPxl > 3 )
   //       {
   //          char lockTooltip[256] = "Waiting lock for ";
   //          ImGui::BeginTooltip();
   //          formatNanosDurationToDisplay(
   //              t.duration,
   //              lockTooltip + strlen( lockTooltip ),
   //              sizeof( lockTooltip ) - strlen( lockTooltip ) );

   //          if ( lockInfo.empty() )
   //          {
   //             // Set a message to warn the user than the thread owning the lock is not part of
   //             // any profiled code
   //             snprintf(
   //                 lockTooltip + strlen( lockTooltip ),
   //                 sizeof( lockTooltip ) - strlen( lockTooltip ),
   //                 "\n  Threads owning the lock were not profiled" );
   //          }
   //          else
   //          {
   //             // Print infos about which threads own the lock
   //             char formattedLockTime[64] = {};
   //             for ( const auto& i : lockInfo )
   //             {
   //                formatNanosDurationToDisplay(
   //                    i.lockDuration, formattedLockTime, sizeof( formattedLockTime ) );
   //                snprintf(
   //                    lockTooltip + strlen( lockTooltip ),
   //                    sizeof( lockTooltip ) - strlen( lockTooltip ),
   //                    "\n  Thread #%u (%s)",
   //                    i.threadIndex,
   //                    formattedLockTime );
   //             }
   //          }
   //          ImGui::TextUnformatted( lockTooltip );
   //          ImGui::EndTooltip();
   //       }

   //       if ( ImGui::IsMouseDoubleClicked( 0 ) )
   //       {
   //          pushNavigationState();
   //          const TimeDuration delta = lockWaits.deltas[t.traceIndex];
   //          frameToAbsoluteTime( lockWaits.ends[t.traceIndex] - delta, delta );
   //       }
   //    }
   // }

   // ImGui::PopStyleColor( 3 );
   // ImGui::PopStyleVar();
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
      _tracks[i].setTrackHeight( heightPerTrack / hop::TimelineTrack::PADDED_TRACE_SIZE );

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
}

} // namespace hop