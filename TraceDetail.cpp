#include "TraceDetail.h"
#include "StringDb.h"
#include "Utils.h"

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
   TraceVecSetItem( hop::TStrPtr_t fName, hop::TLineNb_t lineNb, hop::TStrPtr_t tName, size_t index )
       : fileName( fName ), traceName( tName ), indexInVec( index ), line( lineNb )
   {
   }
   friend bool operator==( const TraceVecSetItem& lhs, const TraceVecSetItem& rhs ) noexcept
   {
      return lhs.fileName == rhs.fileName && lhs.traceName == rhs.traceName && lhs.line == rhs.line;
   }
   hop::TStrPtr_t fileName;
   hop::TStrPtr_t traceName;
   size_t indexInVec;
   hop::TLineNb_t line;
};

template <typename CMP>
static void sortTraceDetailOnName(
    std::vector<hop::TraceDetail>& td,
    const hop::ThreadInfo& threadInfo,
    const hop::StringDb& strDb,
    const CMP& cmp )
{
   HOP_PROF_FUNC();

   std::stable_sort(
       td.begin(),
       td.end(),
       [&threadInfo, &strDb, &cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
          return cmp(
              strcmp(
                  strDb.getString( threadInfo._traces.fctNameIds[lhs.traceIds[0]] ),
                  strDb.getString( threadInfo._traces.fctNameIds[rhs.traceIds[0]] ) ),
              0 );
       } );
}

template <typename MEMBER_T>
static void sortTraceDetailOnMember(
    std::vector<hop::TraceDetail>& td,
    MEMBER_T hop::TraceDetail::*memPtr,
    const hop::ThreadInfo& threadInfo,
    bool descending )
{
   HOP_PROF_FUNC();

   if ( descending )
   {
      auto cmp = std::greater<MEMBER_T>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [&threadInfo, memPtr, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.*memPtr, rhs.*memPtr );
          } );
   }
   else
   {
      auto cmp = std::less<MEMBER_T>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [&threadInfo, memPtr, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.*memPtr, rhs.*memPtr );
          } );
   }
}

static void sortTraceDetailOnCount(
    std::vector<hop::TraceDetail>& td,
    const hop::ThreadInfo& threadInfo,
    bool descending )
{
   HOP_PROF_FUNC();

   if ( descending )
   {
      auto cmp = std::greater<size_t>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [&threadInfo, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
             return cmp( lhs.traceIds.size(), rhs.traceIds.size() );
          } );
   }
   else
   {
      auto cmp = std::less<size_t>();
      std::stable_sort(
          td.begin(),
          td.end(),
          [&threadInfo, cmp]( const hop::TraceDetail& lhs, const hop::TraceDetail& rhs ) {
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
         return std::hash<hop::TLineNb_t>()( t.line ) ^ std::hash<hop::TStrPtr_t>()( t.traceName ) ^ std::hash<hop::TStrPtr_t>()( t.fileName );
      }
   };
}

static std::vector<hop::TraceDetail> mergeTraceDetails( const hop::DisplayableTraces& traces, const std::vector<hop::TraceDetail>& allDetails )
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
         const TimeStamp end = traces.ends[ idx ];
         const TimeStamp delta = traces.deltas[ idx ];
         const TimeStamp start = end - delta;
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
gatherTraceDetails( const hop::DisplayableTraces& traces, size_t traceId )
{
   HOP_PROF_FUNC();

   using namespace hop;
   std::vector<TraceDetail> traceDetails;

   // Find the traces to analyze
   const TimeStamp end = traces.ends[traceId];
   const TimeStamp totalDelta = traces.deltas[traceId];

   size_t firstTraceId = traceId;
   const TimeStamp firstTraceTime = end - totalDelta;
   while ( firstTraceId > 0 && traces.ends[firstTraceId] >= firstTraceTime ) firstTraceId--;

   ++firstTraceId;

   // Early return to prevent crash.
   // TODO : investigate why it sometimes happen...
   if( firstTraceId > traceId ) return traceDetails;

   traceDetails.reserve( traceId - firstTraceId );

   const TDepth_t maxDepth = *std::max_element(
       traces.depths.begin() + firstTraceId, traces.depths.begin() + traceId + 1 );
   std::vector<TimeStamp> accumulatedTimePerDepth( maxDepth + 1, 0 );

   TDepth_t lastDepth = traces.depths[firstTraceId];
   for ( size_t i = firstTraceId; i <= traceId; ++i )
   {
      TimeStamp excTime = traces.deltas[i];
      const TDepth_t curDepth = traces.depths[i];

      if ( curDepth == lastDepth )
      {
         accumulatedTimePerDepth[curDepth] += excTime;
      }
      else if ( curDepth > lastDepth )
      {
         for ( auto i = lastDepth + 1; i < curDepth; ++i ) accumulatedTimePerDepth[i] = 0;

         accumulatedTimePerDepth[curDepth] = excTime;
      }
      else if ( curDepth < lastDepth )
      {
         excTime -= accumulatedTimePerDepth[lastDepth];
         accumulatedTimePerDepth[curDepth] += traces.deltas[i];
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

   assert( std::abs( totalPct - 1.0f ) < 0.01f );
}

namespace hop
{
TraceDetails
createTraceDetails( const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId )
{
   HOP_PROF_FUNC();

   const TimeStamp totalDelta = traces.deltas[traceId];

   std::vector<TraceDetail> traceDetails = mergeTraceDetails( traces, gatherTraceDetails( traces, traceId ) );
   finalizeTraceDetails( traceDetails, totalDelta );

   TraceDetails details;
   details.shouldFocusWindow = true;
   details.threadIndex = threadIndex;
   std::swap( details.details, traceDetails );
   return details;
}

TraceStats createTraceStats(const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId)
{
   const TStrPtr_t fileName = traces.fileNameIds[ traceId ];
   const TStrPtr_t fctName = traces.fctNameIds[ traceId ];
   const TLineNb_t lineNb = traces.lineNbs[ traceId ];

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
         const TimeDuration delta = traces.deltas[i];
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

   return TraceStats{ fctName, count, min, max, median, std::move(displayableDurations), true };
}

TraceDetails createGlobalTraceDetails( const DisplayableTraces& traces, uint32_t threadIndex )
{
   HOP_PROF_FUNC();

   TraceDetails details;

   std::vector<hop::TraceDetail> traceDetails;
   traceDetails.reserve( 1024 );

   TimeDuration totalTime = 0;
   for( size_t i = 0; i < traces.depths.size(); ++i )
   {
      if( traces.depths[i] == 0 )
      {
         totalTime += traces.deltas[i];
         auto details = gatherTraceDetails( traces, i );
         traceDetails.insert( traceDetails.end(), details.begin(), details.end() );
      }
   }

   auto mergedDetails = mergeTraceDetails( traces, traceDetails );
   finalizeTraceDetails( mergedDetails, totalTime );
   details.shouldFocusWindow = true;
   details.threadIndex = threadIndex;
   std::swap( details.details, mergedDetails );
   return details;
}

TraceDetailDrawResult drawTraceDetails(
    TraceDetails& details,
    const std::vector<ThreadInfo>& tracesPerThread,
    const StringDb& strDb )
{
   HOP_PROF_FUNC();

   TraceDetailDrawResult result;
   result.isWindowOpen = details.details.size() > 0;
   if ( details.details.size() > 0 )
   {
      if (details.shouldFocusWindow) ImGui::SetNextWindowFocus();

      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, 0.75f ) );
      // Draw the table header
      const auto buttonCol = ImVec4( 0.20f, 0.20f, 0.20f, 0.0f );
      ImGui::PushStyleColor( ImGuiCol_Button, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonCol );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonCol );
      ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiSetCond_FirstUseEver);
      if ( ImGui::Begin( "Trace Details", &result.isWindowOpen ) )
      {
         ImGui::Columns( 6, "TraceDetailsTable" );
         ImGui::Separator();
         if ( ImGui::Button( "Trace" ) )
         {
            static bool descending = false;
            descending = !descending;

            if ( descending )
            {
               sortTraceDetailOnName(
                   details.details,
                   tracesPerThread[details.threadIndex],
                   strDb,
                   std::greater<int>() );
            }
            else
            {
               sortTraceDetailOnName(
                   details.details, tracesPerThread[details.threadIndex], strDb, std::less<int>() );
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
                tracesPerThread[details.threadIndex],
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
                tracesPerThread[details.threadIndex],
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
                tracesPerThread[details.threadIndex],
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
                tracesPerThread[details.threadIndex],
                descending );
         }
         ImGui::NextColumn();
         if ( ImGui::Button( "Count" ) )
         {
            static bool descending = false;
            descending = !descending;
            sortTraceDetailOnCount(
                details.details, tracesPerThread[details.threadIndex], descending );
         }
         ImGui::NextColumn();
         ImGui::Separator();

         const auto& threadInfo = tracesPerThread[details.threadIndex];
         char traceName[256] = {};
         char traceDuration[128] = {};
         static size_t selected = -1;
         size_t hoveredId = -1;
         for ( size_t i = 0; i < details.details.size(); ++i )
         {
            const size_t traceId = details.details[i].traceIds[0];
            const TStrPtr_t fctIdx = threadInfo._traces.fctNameIds[traceId];
            snprintf( traceName, sizeof( traceName ), "%s", strDb.getString( fctIdx ) );
            if ( ImGui::Selectable(
                     traceName, selected == i, ImGuiSelectableFlags_SpanAllColumns ) )
            {
               selected = i;
            }

            if ( ImGui::IsItemHovered() )
            {
               char fileLineNbStr[128];
               snprintf(
                   fileLineNbStr,
                   sizeof( fileLineNbStr ),
                   "%s:%d",
                   strDb.getString( threadInfo._traces.fileNameIds[traceId] ),
                   threadInfo._traces.lineNbs[traceId] );
               ImGui::BeginTooltip();
               ImGui::TextUnformatted( fileLineNbStr );
               ImGui::EndTooltip();

               hoveredId = i;
            }
            ImGui::NextColumn();
            ImGui::Text( "%3.2f", details.details[i].inclusivePct * 100.0f );
            ImGui::NextColumn();
            formatNanosDurationToDisplay(
                details.details[i].inclusiveTimeInNanos,
                traceDuration,
                sizeof( traceDuration ) );
            ImGui::Text( "%s", traceDuration );
            ImGui::NextColumn();
            ImGui::Text( "%3.2f", details.details[i].exclusivePct * 100.0f );
            ImGui::NextColumn();
            formatNanosDurationToDisplay(
                details.details[i].exclusiveTimeInNanos,
                traceDuration,
                sizeof( traceDuration ) );
            ImGui::Text( "%s", traceDuration );
            ImGui::NextColumn();
            ImGui::Text( "%zu", details.details[i].traceIds.size() );
            ImGui::NextColumn();
         }
         ImGui::Columns( 1 );
         if( hoveredId != (size_t) -1 )
         {
            result.hoveredTraceIds = details.details[hoveredId].traceIds;
         }
      }
      ImGui::End();
      ImGui::PopStyleColor(4);

      result.hoveredThreadIdx = details.threadIndex;
   }

   return result;
}

void drawTraceStats(TraceStats& stats, const std::vector<ThreadInfo>& tracesPerThread, const StringDb& strDb)
{
   if ( stats.open > 0 )
   {
      //if (details.shouldFocusWindow) ImGui::SetNextWindowFocus();

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.20f, 0.20f, 0.75f));
      ImGui::SetNextWindowSize( ImVec2(0, 0) );
      if ( ImGui::Begin("Trace Stats", &stats.open) )
      {
         char minStr[32] = {};
         char maxStr[32] = {};
         char medianStr[32] = {};
         formatNanosDurationToDisplay( stats.min, minStr, sizeof( minStr ) );
         formatNanosDurationToDisplay( stats.max, maxStr, sizeof( maxStr ) );
         formatNanosDurationToDisplay( stats.median, medianStr, sizeof( medianStr ) );
         ImGui::Text("Function : %s\nCount    : %zu\nMin      : %s\nMax      : %s\nMedian   : %s", strDb.getString(stats.fctNameId), stats.count, minStr, maxStr, medianStr );
         ImGui::PlotLines( "", stats.displayableDurations.data(), stats.displayableDurations.size() );
      }
      ImGui::End();
      ImGui::PopStyleColor();
   }
}

}