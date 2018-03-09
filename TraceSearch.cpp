#include "TraceSearch.h"
#include "Timeline.h"
#include "StringDb.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "imgui/imgui.h"

template <typename CMP>
static void sortSearchResOnTime(
    hop::SearchResult& sr,
    const std::vector<hop::ThreadInfo>& threadInfos,
    const CMP& cmp )
{
   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&threadInfos, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              threadInfos[lhs.second].traces.ends[lhs.first] - threadInfos[lhs.second].traces.deltas[lhs.first],
              threadInfos[rhs.second].traces.ends[rhs.first] - threadInfos[rhs.second].traces.deltas[rhs.first] );
       } );
}

template <typename CMP>
static void sortSearchResOnName(
    hop::SearchResult& sr,
    const std::vector<hop::ThreadInfo>& threadInfos,
    const hop::StringDb& strDb,
    const CMP& cmp )
{
   // TODO Fix this as it is not in alphabetic order
   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&threadInfos, &strDb, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              strcmp(
                  strDb.getString( threadInfos[lhs.second].traces.fctNameIds[lhs.first] ),
                  strDb.getString( threadInfos[rhs.second].traces.fctNameIds[rhs.first] ) ),
              0 );
       } );
}

template <typename CMP>
static void sortSearchResOnDuration(
    hop::SearchResult& sr,
    const std::vector<hop::ThreadInfo>& threadInfos,
    const CMP& cmp )
{
   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&threadInfos, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              threadInfos[lhs.second].traces.deltas[lhs.first],
              threadInfos[rhs.second].traces.deltas[rhs.first] );
       } );
}

namespace hop
{

void findTraces( const char* string, const hop::StringDb& strDb, const std::vector< hop::ThreadInfo >& threadInfos, SearchResult& result )
{
   result.stringSearched = string;
   result.matchCount = 0;
   result.tracesIdxThreadIdx.clear();
   result.tracesIdxThreadIdx.reserve(512);

   auto strIds = strDb.findStringIndexMatching( string );
   for( uint32_t threadIdx = 0; threadIdx < threadInfos.size(); ++threadIdx )
   {
       const auto& ti = threadInfos[ threadIdx ];
       for( size_t idx = 0; idx < ti.traces.fctNameIds.size(); ++idx )
       {
          const size_t fctNameId = ti.traces.fctNameIds[ idx ];
          for( auto i : strIds )
          {
             if ( i == fctNameId )
             {
                result.tracesIdxThreadIdx.emplace_back( idx, threadIdx );
                ++result.matchCount;
             }
          }
       }
   }

   // Sort them by duration
   sortSearchResOnDuration( result, threadInfos, std::greater<TimeStamp>() );
}

std::pair< size_t, uint32_t > drawSearchResult( SearchResult& searchRes, const Timeline& timeline, const StringDb& strDb, const std::vector< ThreadInfo >& threadInfos )
{
   ImGui::Text("Found %zu matches", searchRes.matchCount );

   const float entryHeight = ImGui::GetTextLineHeightWithSpacing();
   const float totalEntrySize = entryHeight * (searchRes.tracesIdxThreadIdx.size() + 10);
   ImGui::SetNextWindowContentSize( ImVec2(0, totalEntrySize) );

   // Draw the table header
   const auto buttonCol = ImVec4( 0.20f, 0.20f, 0.20f, 0.0f );
   ImGui::PushStyleColor( ImGuiCol_Button, buttonCol );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonCol );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonCol );
   ImGui::BeginChild("SearchWindow", ImVec2(0,0), true);
   ImGui::Columns( 3, "SearchResult" );
   if( ImGui::Button( "Time" ) )
   {
      static bool descending = false;
      descending = !descending;

      if( descending )
        sortSearchResOnTime( searchRes, threadInfos, std::greater<TimeStamp>() );
      else
        sortSearchResOnTime( searchRes, threadInfos, std::less<TimeStamp>() );
   }
   ImGui::NextColumn();
   if( ImGui::Button( "Trace Name" ) )
   {
      static bool descending = false;
      descending = !descending;

      if( descending )
        sortSearchResOnName( searchRes, threadInfos, strDb, std::greater<int>() );
      else
        sortSearchResOnName( searchRes, threadInfos, strDb, std::less<int>() );
   }
   ImGui::NextColumn();
   if( ImGui::Button( "Duration" ) )
   {
      static bool descending = false;
      descending = !descending;

      if( descending )
        sortSearchResOnDuration( searchRes, threadInfos, std::greater<TimeStamp>() );
      else
        sortSearchResOnDuration( searchRes, threadInfos, std::less<TimeStamp>() );
   }
   ImGui::NextColumn();
   ImGui::Separator();

   // Find out where to start drawing
   const float curScrollY = ImGui::GetScrollY();
   const size_t startIndex = std::max( (int64_t)(curScrollY / entryHeight) - 8ll, 0ll );

   const size_t entryToShow = ImGui::GetWindowHeight() / entryHeight;
   const size_t lastIndex = std::min( startIndex + entryToShow + 10, searchRes.tracesIdxThreadIdx.size() );

   // Add dummy invisible button so the next entry show up where they
   // should in the search table
   const float paddingHeight = std::max( curScrollY - (3.0f * entryHeight), 0.0f );
   ImGui::InvisibleButton( "padding1", ImVec2( 0.0f, paddingHeight ) );
   ImGui::NextColumn();
   ImGui::InvisibleButton( "padding2", ImVec2( 0.0f, paddingHeight ) );
   ImGui::NextColumn();
   ImGui::InvisibleButton( "padding3", ImVec2( 0.0f, paddingHeight ) );
   ImGui::NextColumn();

   static size_t selected = -1;
   const TimeStamp absoluteStartTime = timeline.absoluteStartTime();
   const auto totalMicrosInScreen = timeline.microsToDisplay();
   char traceTime[64] = {};
   char traceDuration[64] = {};
   bool selectedSomething = false;
   for( size_t i = startIndex; i < lastIndex; ++i )
   {
       ImGui::PushID(i);
       const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[i];
       const auto& ti = threadInfos[ traceIdThreadId.second ];
       const size_t traceId = traceIdThreadId.first;
       const TimeStamp delta = ti.traces.deltas[ traceId ];

       hop::formatMicrosTimepointToDisplay(
           (ti.traces.ends[ traceId ] - delta - absoluteStartTime) * 0.001f,
           totalMicrosInScreen,
           traceTime,
           sizeof( traceTime ) );
       if ( ImGui::Selectable( traceTime, selected == i, ImGuiSelectableFlags_SpanAllColumns ) )
       {
          selected = i;
          selectedSomething = true;
       }
       ImGui::NextColumn();
       ImGui::Text( "%s", strDb.getString( ti.traces.fctNameIds[ traceId ] ) );
       ImGui::NextColumn();
       hop::formatMicrosDurationToDisplay(
           delta * 0.001f,
           traceDuration,
           sizeof( traceDuration ) );
       ImGui::Text( "%s", traceDuration );
       ImGui::NextColumn();
       ImGui::PopID();
   }
   ImGui::Columns( 1 );
   ImGui::EndChild();
   ImGui::PopStyleColor( 3 );

   size_t selectedTraceId = -1;
   uint32_t selectedThreadId = -1;
   if( selectedSomething )
   {
      const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[selected];
      selectedTraceId = traceIdThreadId.first;
      selectedThreadId = traceIdThreadId.second;
   }

   return std::make_pair( selectedTraceId, selectedThreadId );
}

} // namespace hop