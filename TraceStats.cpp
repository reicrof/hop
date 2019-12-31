#include "TraceStats.h"

#include "Options.h"  // window opacity

#include "common/TraceData.h"
#include "common/TimelineTrack.h"
#include "common/StringDb.h"
#include "common/Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include <cassert>
#include <cmath>

namespace
{
struct TraceVecSetItem
{
   TraceVecSetItem( hop::StrPtr_t fName, hop::LineNb_t lineNb, hop::StrPtr_t tName, size_t index )
       : fileName( fName ), traceName( tName ), indexInVec( index ), line( lineNb )
   {
   }
   friend bool operator==( const TraceVecSetItem& lhs, const TraceVecSetItem& rhs ) noexcept
   {
      return lhs.fileName == rhs.fileName && lhs.traceName == rhs.traceName && lhs.line == rhs.line;
   }
   hop::StrPtr_t fileName;
   hop::StrPtr_t traceName;
   size_t indexInVec;
   hop::LineNb_t line;
};

template <typename CMP>
static void sortTraceDetailOnName(
    std::vector<hop::TraceDetail>& td,
    const hop::TimelineTrack& timelineTrack,
    const hop::StringDb& strDb,
    const CMP& cmp )
{
   HOP_PROF_FUNC();

   std::stable_sort(
       td.begin(),
       td.end(),
       [&timelineTrack, &strDb, &cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
          return cmp(
              strcmp(
                  strDb.getString( timelineTrack._traces.fctNameIds[lhs.traceIds[0]] ),
                  strDb.getString( timelineTrack._traces.fctNameIds[rhs.traceIds[0]] ) ),
              0 );
       } );
}

template <typename MEMBER_T>
static void sortTraceDetailOnMember(
    std::vector<hop::TraceDetail>& td,
    MEMBER_T hop::TraceDetail::*memPtr,
    bool descending )
{
   HOP_PROF_FUNC();

   if ( descending )
   {
      auto cmp = std::greater<MEMBER_T>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [memPtr, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.*memPtr, rhs.*memPtr );
          } );
   }
   else
   {
      auto cmp = std::less<MEMBER_T>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [memPtr, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.*memPtr, rhs.*memPtr );
          } );
   }
}

static void sortTraceDetailOnCount(
    std::vector<hop::TraceDetail>& td,
    bool descending )
{
   HOP_PROF_FUNC();

   if ( descending )
   {
      auto cmp = std::greater<size_t>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.traceIds.size(), rhs.traceIds.size() );
          } );
   }
   else
   {
      auto cmp = std::less<size_t>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.traceIds.size(), rhs.traceIds.size() );
          } );
   }
}
}

namespace std
{
   template <>
   struct hash<TraceVecSetItem>
   {
      size_t operator()( const TraceVecSetItem& t ) const
      {
         return std::hash<hop::LineNb_t>()( t.line ) ^ std::hash<hop::StrPtr_t>()( t.traceName ) ^ std::hash<hop::StrPtr_t>()( t.fileName );
      }
   };
}

static std::vector<hop::TraceDetail> mergeTraceDetails( const hop::TraceData& traces, const std::vector<hop::TraceDetail>& allDetails )
{
   HOP_PROF_FUNC();

   using namespace hop;
   std::vector<TraceDetail> mergedDetails;
   mergedDetails.reserve( allDetails.size() / 2 );

   std::unordered_set<TraceVecSetItem> uniqueTraces;
   uniqueTraces.reserve( allDetails.size() / 2 );

   for ( const auto& t : allDetails )
   {
      const auto insertRes = uniqueTraces.insert( TraceVecSetItem(
          traces.fileNameIds[t.traceIds[0]],
          traces.lineNbs[t.traceIds[0]],
          traces.fctNameIds[t.traceIds[0]],
          mergedDetails.size() ) );
      if ( insertRes.second )
      {
         // New entry
         mergedDetails.emplace_back( t );
      }
      else
      {
         auto& d = mergedDetails[insertRes.first->indexInVec];
         d.exclusiveTimeInNanos += t.exclusiveTimeInNanos;
         d.inclusiveTimeInNanos += t.inclusiveTimeInNanos;
         d.traceIds.insert( d.traceIds.end(), t.traceIds.begin(), t.traceIds.end() );
      }
   }

   // Compute the inclusive time. Tje inclusive time can be seen as the union of the time
   // a function spent, regardless of its depth
   std::vector< TimeStamp > startTimes, endTimes;
   startTimes.reserve( 128 );
   endTimes.reserve( 128 );
   for( auto& t : mergedDetails )
   {
      for( auto idx : t.traceIds )
      {
         // Try to merge it with exsiting time
         bool merged = false;
         const TimeStamp end = traces.entries.ends[ idx ];
         const TimeStamp start = traces.entries.starts[ idx ];
         for( size_t i = 0; i < endTimes.size(); ++i )
         {
            const TimeStamp curStart = startTimes[i];
            const TimeStamp curEnd = endTimes[i];

            // If it is fully contained in the trace, there is nothing to do
            if( end <= curEnd && start >= curStart )
            {
               merged = true;
               break;
            }

            // If it started inside the cur trace but finished later, merge
            // the end time
            if( end > curEnd && start <= curEnd )
            {
               merged = true;
               endTimes[i] = end;
               startTimes[i] = std::min( start, curStart );
               break;
            }

            // If it started before the cur trace and ended inside of it,
            // merge the start time
            if( start < curStart && end >= curStart )
            {
               merged = true;
               startTimes[i] = start;
               endTimes[i] = std::max( end, curEnd );
               break;
            }
         }

         // If it was not merged, it means it is disjoint from all other
         // traces
         if( !merged )
         {
            startTimes.push_back( start );
            endTimes.push_back( end );
         }
      }

      // Now that we have the union of the start/end times, compute
      // the sum of the elapsed time.
      TimeStamp inclusiveTime = 0;
      for( size_t i = 0; i < startTimes.size(); ++i )
      {
         inclusiveTime += endTimes[i] - startTimes[i];
      }
      t.inclusiveTimeInNanos = inclusiveTime;

      startTimes.clear();
      endTimes.clear();
   }

   return mergedDetails;
}

static std::vector<hop::TraceDetail>
gatherTraceDetails( const hop::TraceData& traces, size_t traceId )
{
   HOP_PROF_FUNC();

   using namespace hop;
   std::vector<TraceDetail> traceDetails;

   // Find the traces to analyze
   const TimeStamp firstTraceTime = traces.entries.starts[traceId];

   size_t firstTraceId = traceId;
   while ( firstTraceId > 0 && traces.entries.ends[firstTraceId] >= firstTraceTime ) firstTraceId--;

   ++firstTraceId;

   // Early return to prevent crash.
   // TODO : investigate why it sometimes happen...
   if( firstTraceId > traceId ) return traceDetails;

   traceDetails.reserve( traceId - firstTraceId );

   const Depth_t maxDepth = *std::max_element(
       traces.entries.depths.begin() + firstTraceId, traces.entries.depths.begin() + traceId + 1 );
   std::vector<TimeStamp> accumulatedTimePerDepth( maxDepth + 1, 0 );

   Depth_t lastDepth = traces.entries.depths[firstTraceId];
   for ( size_t i = firstTraceId; i <= traceId; ++i )
   {
      const TimeStamp delta  = traces.entries.ends[i] - traces.entries.starts[i];
      const Depth_t curDepth = traces.entries.depths[i];
      TimeStamp excTime = delta;

      if ( curDepth == lastDepth )
      {
         accumulatedTimePerDepth[curDepth] += excTime;
      }
      else if ( curDepth > lastDepth )
      {
         for ( auto j = lastDepth + 1; j < curDepth; ++j ) accumulatedTimePerDepth[j] = 0;

         accumulatedTimePerDepth[curDepth] = excTime;
      }
      else if ( curDepth < lastDepth )
      {
         excTime -= accumulatedTimePerDepth[lastDepth];
         accumulatedTimePerDepth[curDepth] += delta;
      }

      lastDepth = curDepth;

      traceDetails.emplace_back( TraceDetail( i, excTime ) );
   }

   return traceDetails;
}

static void finalizeTraceDetails(
    std::vector<hop::TraceDetail>& details,
    hop::TimeDuration totalTime )
{
   HOP_PROF_FUNC();

   using namespace hop;
   // Adjust the percentage
   float totalPct = 0.0f;
   for ( auto& t : details )
   {
      t.exclusivePct = (double)t.exclusiveTimeInNanos / (double)totalTime;
      t.inclusivePct = (double)t.inclusiveTimeInNanos / (double)totalTime;
      totalPct += t.exclusivePct;
   }

   // Sort them by exclusive percent
   std::sort( details.begin(), details.end(), []( const TraceDetail& lhs, const TraceDetail& rhs ) {
      return lhs.exclusivePct > rhs.exclusivePct;
   } );

   assert( std::abs( totalPct - 1.0f ) < 0.01f || details.empty() );
}

namespace hop
{
TraceDetails
createTraceDetails( const TraceData& traces, uint32_t threadIndex, size_t traceId )
{
   HOP_PROF_FUNC();

   const TimeStamp totalDelta = traces.entries.ends[traceId] - traces.entries.starts[traceId];

   std::vector<TraceDetail> traceDetails = mergeTraceDetails( traces, gatherTraceDetails( traces, traceId ) );
   finalizeTraceDetails( traceDetails, totalDelta );

   TraceDetails details;
   details.open = true;
   details.shouldFocusWindow = true;
   details.threadIndex = threadIndex;
   std::swap( details.details, traceDetails );
   return details;
}

 TraceStats createTraceStats(const TraceData& traces, uint32_t, size_t traceId)
 {
    const StrPtr_t fileName = traces.fileNameIds[ traceId ];
    const StrPtr_t fctName = traces.fctNameIds[ traceId ];
    const LineNb_t lineNb = traces.lineNbs[ traceId ];

    std::vector<float> displayableDurations;
    std::vector<float> medianValues;
    displayableDurations.reserve( 256 );
    medianValues.reserve( 256 );
    TimeDuration min = std::numeric_limits< TimeDuration >::max();
    TimeDuration max = 0;
    TimeDuration median = 0;
    size_t count = 0;

    for (size_t i = 0; i < traces.fileNameIds.size(); ++i)
    {
       if (traces.fileNameIds[i] == fileName && traces.fctNameIds[i] == fctName && traces.lineNbs[i] == lineNb)
       {
          const TimeDuration delta = traces.entries.ends[i] - traces.entries.starts[i];
          displayableDurations.push_back( (float) delta );
          medianValues.push_back( delta );
          min = std::min( min, delta );
          max = std::max( max, delta );
          ++count;
       }
    }

    if( !medianValues.empty() )
    {
       std::nth_element(medianValues.begin(), medianValues.begin() + medianValues.size() / 2, medianValues.end());
       median = medianValues[ medianValues.size() / 2 ];
    }

    return TraceStats{ fctName, count, min, max, median, std::move(displayableDurations), true, true };
 }

TraceDetails createGlobalTraceDetails( const TraceData& traces, uint32_t threadIndex )
{
   HOP_PROF_FUNC();

   TraceDetails details = {};

   std::vector<hop::TraceDetail> traceDetails;
   traceDetails.reserve( 1024 );

   TimeDuration totalTime = 0;
   for( size_t i = 0; i < traces.entries.depths.size(); ++i )
   {
      if( traces.entries.depths[i] == 0 )
      {
         totalTime += traces.entries.ends[i] - traces.entries.starts[i];
         auto details = gatherTraceDetails( traces, i );
         traceDetails.insert( traceDetails.end(), details.begin(), details.end() );
      }
   }

   auto mergedDetails = mergeTraceDetails( traces, traceDetails );
   finalizeTraceDetails( mergedDetails, totalTime );
   details.shouldFocusWindow = true;
   details.open = true;
   details.threadIndex = threadIndex;
   std::swap( details.details, mergedDetails );
   return details;
}

TraceDetailDrawResult drawTraceDetails(
    TraceDetails& details,
    const std::vector<TimelineTrack>& tracks,
    const StringDb& strDb,
    bool drawAsCycles )
{
   HOP_PROF_FUNC();

   static constexpr float pctColumnWidth = 65.0f;
   static constexpr float timeColumnWidth = 90.0f;

   TraceDetailDrawResult result = { std::vector< size_t >(), 0, false };
   if ( details.open )
   {
      if( details.shouldFocusWindow )
      {
         ImGui::SetNextWindowFocus();
         ImGui::SetNextWindowCollapsed( false );
         details.shouldFocusWindow = false;
      }

      const float wndOpacity = hop::options::windowOpacity();
      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );
      // Draw the table header
      const auto buttonCol = ImVec4( 0.20f, 0.20f, 0.20f, 0.0f );
      ImGui::PushStyleColor( ImGuiCol_Button, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonCol );
      ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiSetCond_FirstUseEver);
      if ( ImGui::Begin( "Trace Details Window", &details.open ) )
      {
         ImGui::Columns( 6, "TraceDetailsTable" );
         ImGui::SetColumnWidth( 0, ImGui::GetWindowWidth() - 400 );
         ImGui::SetColumnWidth( 1, 80 );
         ImGui::SetColumnWidth( 2, timeColumnWidth );
         ImGui::SetColumnWidth( 3, pctColumnWidth );
         ImGui::SetColumnWidth( 4, timeColumnWidth );
         ImGui::SetColumnWidth( 5, pctColumnWidth );
         ImGui::Separator();
         if ( ImGui::Button( "Trace" ) )
         {
            static bool descending = false;
            descending = !descending;

            if ( descending )
            {
               sortTraceDetailOnName(
                   details.details,
                   tracks[details.threadIndex],
                   strDb,
                   std::greater<int>() );
            }
            else
            {
               sortTraceDetailOnName(
                   details.details, tracks[details.threadIndex], strDb, std::less<int>() );
            }
         }

         ImGui::NextColumn();
         if ( ImGui::Button( "Incl. %" ) )
         {
            static bool descending = false;
            descending = !descending;

            sortTraceDetailOnMember(
                details.details,
                &TraceDetail::inclusivePct,
                descending );
         }

         ImGui::NextColumn();
         if ( ImGui::Button( "Incl Time" ) )
         {
            static bool descending = false;
            descending = !descending;

            sortTraceDetailOnMember(
                details.details,
                &TraceDetail::inclusiveTimeInNanos,
                descending );
         }

         ImGui::NextColumn();
         if ( ImGui::Button( "Excl. %" ) )
         {
            static bool descending = false;
            descending = !descending;

            sortTraceDetailOnMember(
                details.details,
                &TraceDetail::exclusivePct,
                descending );
         }

         ImGui::NextColumn();
         if ( ImGui::Button( "Excl Time" ) )
         {
            static bool descending = false;
            descending = !descending;

            sortTraceDetailOnMember(
                details.details,
                &TraceDetail::exclusiveTimeInNanos,
                descending );
         }

         ImGui::NextColumn();
         if ( ImGui::Button( "Count" ) )
         {
            static bool descending = false;
            descending = !descending;
            sortTraceDetailOnCount( details.details, descending );
         }

         ImGui::NextColumn();
         ImGui::Separator();

         const auto& track = tracks[details.threadIndex];
         char traceName[256] = {};
         char traceDuration[128] = {};
         static size_t selected = -1;
         size_t hoveredId = -1;
         bool selectedSomething = false;
         for ( size_t i = 0; i < details.details.size(); ++i )
         {
            const size_t traceId = details.details[i].traceIds[0];
            const StrPtr_t fctIdx = track._traces.fctNameIds[traceId];
            snprintf( traceName, sizeof( traceName ), "%s", strDb.getString( fctIdx ) );
            if ( ImGui::Selectable(
                     traceName, selected == i, ImGuiSelectableFlags_SpanAllColumns ) )
            {
               selected = i;
               selectedSomething = true;
            }

            if ( ImGui::IsItemHovered() )
            {
               char fileLineNbStr[128];
               snprintf(
                   fileLineNbStr,
                   sizeof( fileLineNbStr ),
                   "%s:%d",
                   strDb.getString( track._traces.fileNameIds[traceId] ),
                   track._traces.lineNbs[traceId] );
               ImGui::BeginTooltip();
               ImGui::TextUnformatted( fileLineNbStr );
               ImGui::EndTooltip();

               hoveredId = i;
            }
            ImGui::NextColumn();
            ImGui::Text( "%3.2f", details.details[i].inclusivePct * 100.0f );
            ImGui::NextColumn();
            formatCyclesDurationToDisplay(
                details.details[i].inclusiveTimeInNanos,
                traceDuration,
                sizeof( traceDuration ),
                drawAsCycles );
            ImGui::Text( "%s", traceDuration );
            ImGui::NextColumn();
            ImGui::Text( "%3.2f", details.details[i].exclusivePct * 100.0f );
            ImGui::NextColumn();
            formatCyclesDurationToDisplay(
                details.details[i].exclusiveTimeInNanos,
                traceDuration,
                sizeof( traceDuration ),
                drawAsCycles );
            ImGui::Text( "%s", traceDuration );
            ImGui::NextColumn();
            ImGui::Text( "%zu", details.details[i].traceIds.size() );
            ImGui::NextColumn();
         }
         ImGui::Columns( 1 );
         if( hoveredId != (size_t) -1 )
         {
            result.hoveredTraceIds = details.details[hoveredId].traceIds;
            result.clicked = selectedSomething;
         }
      }
      ImGui::End();
      ImGui::PopStyleColor(4);

      result.hoveredThreadIdx = details.threadIndex;
   }

   return result;
}

void drawTraceStats( TraceStats& stats, const StringDb& strDb, bool drawAsCycles )
{
   if ( stats.open > 0 )
   {
      if (stats.focus)
      {
         ImGui::SetNextWindowFocus();
         stats.focus = false;
      }

      const float wndOpacity = hop::options::windowOpacity();
      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.20f, 0.20f, wndOpacity));
      ImGui::SetNextWindowSize( ImVec2(0, 0) );
      if ( ImGui::Begin("Trace Stats", &stats.open) )
      {
         char minStr[32] = {};
         char maxStr[32] = {};
         char medianStr[32] = {};
         formatCyclesDurationToDisplay( stats.max, maxStr, sizeof( maxStr ), drawAsCycles );
         formatCyclesDurationToDisplay( stats.min, minStr, sizeof( minStr ), drawAsCycles );
         formatCyclesDurationToDisplay( stats.median, medianStr, sizeof( medianStr ), drawAsCycles );
         ImGui::Text("Function : %s\nCount    : %zu\nMin      : %s\nMax      : %s\nMedian   : %s", strDb.getString(stats.fctNameId), stats.count, minStr, maxStr, medianStr );
         ImGui::PlotLines( "", stats.displayableDurations.data(), stats.displayableDurations.size() );
      }
      ImGui::End();
      ImGui::PopStyleColor();
   }
}

void clearTraceDetails( TraceDetails& traceDetail )
{
   traceDetail.details.clear();
   traceDetail.threadIndex = 0;
   traceDetail.open = false;
   traceDetail.shouldFocusWindow = false;
}

void clearTraceStats( TraceStats& stats )
{
   stats = TraceStats{ 0, 0, 0, 0, 0, std::vector< float >(), false, false };
}

}