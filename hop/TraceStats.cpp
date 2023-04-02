#include "hop/TraceStats.h"

#include "hop/Options.h"  // window opacity

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

   (void)totalPct;
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

TraceStats createTraceStats( const TraceData& traces, uint32_t, size_t traceId )
{
   const StrPtr_t fileName = traces.fileNameIds[traceId];
   const StrPtr_t fctName             = traces.fctNameIds[traceId];
   const LineNb_t lineNb              = traces.lineNbs[traceId];

   TraceStats stats;
   stats.min       = std::numeric_limits<TimeDuration>::max();
   stats.max       = 0;
   stats.median    = 0;
   stats.count     = 0;
   stats.fctNameId = fctName;
   stats.displayableDurations.reserve( 256 );
   std::vector<float> medianValues;
   medianValues.reserve( 256 );

   for( size_t i = 0; i < traces.fileNameIds.size(); ++i )
   {
      if( traces.fileNameIds[i] == fileName && traces.fctNameIds[i] == fctName &&
          traces.lineNbs[i] == lineNb )
      {
         const TimeDuration delta = traces.entries.ends[i] - traces.entries.starts[i];
         stats.displayableDurations.push_back( (float)delta );
         medianValues.push_back( delta );
         stats.min = std::min( stats.min, delta );
         stats.max = std::max( stats.max, delta );
         ++stats.count;
      }
   }

   if( !medianValues.empty() )
   {
      std::nth_element(
          medianValues.begin(),
          medianValues.begin() + medianValues.size() / 2,
          medianValues.end() );
      stats.median = medianValues[medianValues.size() / 2];
   }

   stats.open = stats.focus = true;
   return stats;
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
    bool drawAsCycles,
    float cpuFreqGHz )
{
   HOP_PROF_FUNC();

   TraceDetailDrawResult result = { std::vector<size_t>(), 0, false };
   if( details.open )
   {
      if( details.shouldFocusWindow )
      {
         ImGui::SetNextWindowFocus();
         ImGui::SetNextWindowCollapsed( false );
         details.shouldFocusWindow = false;
      }

      ImVec2 size = ImGui::GetIO().DisplaySize * ImVec2( 0.6f, 0.5f );
      ImVec2 pos = ImGui::GetIO().DisplaySize * ImVec2( 0.5f, 0.5f );
      ImGui::SetNextWindowSize( size, ImGuiCond_Appearing );
      ImGui::SetNextWindowPos( pos, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

      const float wndOpacity = hop::options::windowOpacity();
      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );
      // Draw the table header
      const auto buttonCol = ImVec4( 0.20f, 0.20f, 0.20f, 0.0f );
      ImGui::PushStyleColor( ImGuiCol_Button, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonCol );
      if( ImGui::Begin( "Trace Details Window", &details.open ) )
      {
         uint32_t tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                               ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter;
         if( ImGui::BeginTable( "TraceDetailsTable", 6, tableFlags ) )
         {
            ImGui::TableSetupScrollFreeze( 0, 1 );  // Make top row always visible
            ImGui::TableSetupColumn( "Trace", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Incl. %", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Incl Time", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Excl. %", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Excl. Time", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Count", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            if( ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs() )
            {
               if( sortSpec->SpecsDirty && sortSpec->SpecsCount > 0 )
               {
                  bool descending =
                      sortSpec->Specs[0].SortDirection == ImGuiSortDirection_Descending;
                  switch( sortSpec->Specs[0].ColumnIndex )
                  {
                     case 0: /* Name */
                        if( descending )
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
                               details.details,
                               tracks[details.threadIndex],
                               strDb,
                               std::less<int>() );
                        }
                        break;
                     case 1: /* Inclusive % */
                        sortTraceDetailOnMember(
                            details.details, &TraceDetail::inclusivePct, descending );
                        break;
                     case 2: /* Inclusive Time */
                        sortTraceDetailOnMember(
                            details.details, &TraceDetail::inclusiveTimeInNanos, descending );
                        break;
                     case 3: /* Exclusive % */
                        sortTraceDetailOnMember(
                            details.details, &TraceDetail::exclusivePct, descending );
                        break;
                     case 4: /* Exclusive Time */
                        sortTraceDetailOnMember(
                            details.details, &TraceDetail::exclusiveTimeInNanos, descending );
                        break;
                     case 5:
                        sortTraceDetailOnCount( details.details, descending );
                        break;
                  }

                  sortSpec->SpecsDirty = false;
               }
            }

            const auto& track       = tracks[details.threadIndex];
            char traceName[256]     = {};
            char traceDuration[128] = {};
            static size_t selected  = -1;
            size_t hoveredId        = -1;
            bool selectedSomething  = false;
            for( size_t i = 0; i < details.details.size(); ++i )
            {
               ImGui::TableNextRow();
               ImGui::TableSetColumnIndex( 0 );
               const size_t traceId  = details.details[i].traceIds[0];
               const StrPtr_t fctIdx = track._traces.fctNameIds[traceId];
               snprintf( traceName, sizeof( traceName ), "%s", strDb.getString( fctIdx ) );
               if( ImGui::Selectable(
                       traceName, selected == i, ImGuiSelectableFlags_SpanAllColumns ) )
               {
                  selected          = i;
                  selectedSomething = true;
               }

               if( ImGui::IsItemHovered() )
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

               ImGui::TableSetColumnIndex( 1 );
               ImGui::Text( "%3.2f", details.details[i].inclusivePct * 100.0f );

               ImGui::TableSetColumnIndex( 2 );
               formatCyclesDurationToDisplay(
                   details.details[i].inclusiveTimeInNanos,
                   traceDuration,
                   sizeof( traceDuration ),
                   drawAsCycles,
                   cpuFreqGHz );
               ImGui::Text( "%s", traceDuration );

               ImGui::TableSetColumnIndex( 3 );
               ImGui::Text( "%3.2f", details.details[i].exclusivePct * 100.0f );

               ImGui::TableSetColumnIndex( 4 );
               formatCyclesDurationToDisplay(
                   details.details[i].exclusiveTimeInNanos,
                   traceDuration,
                   sizeof( traceDuration ),
                   drawAsCycles,
                   cpuFreqGHz );
               ImGui::Text( "%s", traceDuration );

               ImGui::TableSetColumnIndex( 5 );
               ImGui::Text( "%zu", details.details[i].traceIds.size() );
               if( hoveredId != (size_t)-1 )
               {
                  result.hoveredTraceIds = details.details[hoveredId].traceIds;
                  result.clicked         = selectedSomething;
               }
            }
            ImGui::EndTable();
         }
      }
      ImGui::End();
      ImGui::PopStyleColor( 4 );

      result.hoveredThreadIdx = details.threadIndex;
   }

   return result;
}

void drawTraceStats( TraceStats& stats, const StringDb& strDb, bool drawAsCycles, float cpuFreqGHz )
{
   if ( stats.open )
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
         formatCyclesDurationToDisplay( stats.max, maxStr, sizeof( maxStr ), drawAsCycles, cpuFreqGHz );
         formatCyclesDurationToDisplay( stats.min, minStr, sizeof( minStr ), drawAsCycles, cpuFreqGHz );
         formatCyclesDurationToDisplay( stats.median, medianStr, sizeof( medianStr ), drawAsCycles, cpuFreqGHz );
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
   stats.fctNameId       = 0;
   stats.count           = 0;
   stats.min             = 0;
   stats.max             = 0;
   stats.median          = 0;
   stats.open            = false;
   stats.focus           = false;
   stats.displayableDurations.clear();
}

}
