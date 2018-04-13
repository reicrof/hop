#include "TraceDetail.h"
#include "StringDb.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
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
         d.deltaTimeInNanos += t.deltaTimeInNanos;
         d.traceIds.insert( d.traceIds.end(), t.traceIds.begin(), t.traceIds.end() );
      }
   }

   return mergedDetails;
}

static std::vector<hop::TraceDetail>
gatherTraceDetails( const hop::DisplayableTraces& traces, size_t traceId )
{
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
      TimeStamp traceDelta = traces.deltas[i];
      const TDepth_t curDepth = traces.depths[i];

      if ( curDepth == lastDepth )
      {
         accumulatedTimePerDepth[curDepth] += traceDelta;
      }
      else if ( curDepth > lastDepth )
      {
         for ( auto i = lastDepth + 1; i < curDepth; ++i ) accumulatedTimePerDepth[i] = 0;

         accumulatedTimePerDepth[curDepth] = traceDelta;
      }
      else if ( curDepth < lastDepth )
      {
         traceDelta -= accumulatedTimePerDepth[lastDepth];
         accumulatedTimePerDepth[curDepth] += traces.deltas[i];
      }

      lastDepth = curDepth;

      traceDetails.emplace_back( TraceDetail( i, traceDelta ) );
   }

   return traceDetails;
}

static void finalizeTraceDetails(
    std::vector<hop::TraceDetail>& details,
    hop::TimeDuration totalTime )
{
   using namespace hop;
   // Adjust the percentage
   float totalPct = 0.0f;
   for ( auto& t : details )
   {
      t.exclusivePct = (double)t.deltaTimeInNanos / (double)totalTime;
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
   const TimeStamp totalDelta = traces.deltas[traceId];

   std::vector<TraceDetail> traceDetails = mergeTraceDetails( traces, gatherTraceDetails( traces, traceId ) );
   finalizeTraceDetails( traceDetails, totalDelta );

   TraceDetails details;
   details.shouldFocusWindow = true;
   details.threadIndex = threadIndex;
   std::swap( details.details, traceDetails );
   return details;
}

TraceDetails createGlobalTraceDetails( const DisplayableTraces& traces, uint32_t threadIndex )
{
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
    const TraceDetails& details,
    const std::vector<ThreadInfo>& tracesPerThread,
    const StringDb& strDb )
{
   TraceDetailDrawResult result;
   result.isWindowOpen = details.details.size() > 0;
   if ( details.details.size() > 0 )
   {
      if (details.shouldFocusWindow) ImGui::SetNextWindowFocus();

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.20f, 0.20f, 0.75f));
      ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiSetCond_FirstUseEver);
      if ( ImGui::Begin( "Trace Details", &result.isWindowOpen ) )
      {
         const auto& threadInfo = tracesPerThread[details.threadIndex];

         ImGui::Columns( 4, "TraceDetailsTable" );  // 4-ways, with border
         ImGui::Separator();
         ImGui::Text( "Trace" );
         ImGui::NextColumn();
         ImGui::Text( "Excl. %%" );
         ImGui::NextColumn();
         ImGui::Text( "Excl Time" );
         ImGui::NextColumn();
         ImGui::Text( "Count" );
         ImGui::NextColumn();
         ImGui::Separator();

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
            ImGui::Text( "%3.2f", details.details[i].exclusivePct * 100.0f );
            ImGui::NextColumn();
            formatNanosDurationToDisplay(
                details.details[i].deltaTimeInNanos,
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
      ImGui::PopStyleColor();

      result.hoveredThreadIdx = details.threadIndex;
   }

   return result;
}
}