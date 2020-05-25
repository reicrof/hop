#include "hop/SearchWindow.h"
#include "hop/Options.h" // window opacity
#include "hop/TimelineInfo.h"
#include "hop/Stats.h"

#include "common/StringDb.h"
#include "common/TimelineTrack.h"
#include "common/Utils.h"

#include "imgui/imgui.h"

#include <functional> //std::greater

template <typename CMP>
static void sortSearchResOnTime(
    hop::SearchResult& sr,
    const std::vector<hop::TimelineTrack>& tracks,
    const CMP& cmp )
{
   HOP_PROF_FUNC();

   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&tracks, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              tracks[lhs.second]._traces.entries.starts[lhs.first],
              tracks[rhs.second]._traces.entries.starts[rhs.first] );
       } );
}

template <typename CMP>
static void sortSearchResOnName(
    hop::SearchResult& sr,
    const std::vector<hop::TimelineTrack>& tracks,
    const hop::StringDb& strDb,
    const CMP& cmp )
{
   HOP_PROF_FUNC();

   // TODO Fix this as it is not in alphabetic order
   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&tracks, &strDb, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              strcmp(
                  strDb.getString( tracks[lhs.second]._traces.fctNameIds[lhs.first] ),
                  strDb.getString( tracks[rhs.second]._traces.fctNameIds[rhs.first] ) ),
              0 );
       } );
}

template <typename CMP>
static void sortSearchResOnDuration(
    hop::SearchResult& sr,
    const std::vector<hop::TimelineTrack>& tracks,
    const CMP& cmp )
{
   HOP_PROF_FUNC();

   std::stable_sort(
       sr.tracesIdxThreadIdx.begin(),
       sr.tracesIdxThreadIdx.end(),
       [&tracks, &cmp](
           const std::pair<size_t, uint32_t>& lhs, const std::pair<size_t, uint32_t>& rhs ) {
          return cmp(
              tracks[lhs.second]._traces.entries.ends[lhs.first] - tracks[lhs.second]._traces.entries.starts[lhs.first],
              tracks[rhs.second]._traces.entries.ends[rhs.first] - tracks[rhs.second]._traces.entries.starts[rhs.first] );
       } );
}

namespace hop
{
void findTraces( const char* string, const StringDb& strDb, const std::vector<hop::TimelineTrack>& tracks, SearchResult& result )
{
   HOP_PROF_FUNC();

   result.stringSearched = string;
   result.matchCount = 0;
   result.tracesIdxThreadIdx.clear();
   result.tracesIdxThreadIdx.reserve(512);

   auto strIds = strDb.findStringIndexMatching( string );
   for( uint32_t threadIdx = 0; threadIdx < tracks.size(); ++threadIdx )
   {
       const auto& ti = tracks[ threadIdx ];
       for( size_t idx = 0; idx < ti._traces.fctNameIds.size(); ++idx )
       {
          const size_t fctNameId = ti._traces.fctNameIds[ idx ];
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
   sortSearchResOnDuration( result, tracks, std::greater<TimeStamp>() );
}

SearchSelection drawSearchResult(
   SearchResult& searchRes,
   const StringDb& strDb,
   const TimelineInfo& tlInfo,
   const std::vector<hop::TimelineTrack>& tracks,
   float cpuFreqGHz )
{
   HOP_PROF_FUNC();

   bool inputFocus = false;
   if ( searchRes.focusSearchWindow && searchRes.searchWindowOpen )
   {
      ImGui::SetNextWindowFocus();
      ImGui::SetNextWindowCollapsed( false );
      inputFocus = true;
      searchRes.focusSearchWindow = false;
   }

   size_t selectedTraceId = -1;
   size_t hoveredTraceId = -1;
   uint32_t selectedThreadId = -1;
   uint32_t hoveredThreadId = -1;

   if ( searchRes.searchWindowOpen )
   {
      const float wndOpacity = hop::options::windowOpacity();
      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );
      ImGui::SetNextWindowSize( ImVec2( 600, 300 ), ImGuiSetCond_FirstUseEver );
      if ( ImGui::Begin( "Search Window", &searchRes.searchWindowOpen ) )
      {
         static char input[512];

         if ( inputFocus ) ImGui::SetKeyboardFocusHere();

         if ( ImGui::InputText(
                  "Search",
                  input,
                  sizeof( input ),
                  ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue ) &&
              strlen( input ) > 0 )
         {
            const auto startSearch = std::chrono::system_clock::now();

            findTraces( input, strDb, tracks, searchRes );

            const auto endSearch = std::chrono::system_clock::now();
            hop::g_stats.searchTimeMs =
                std::chrono::duration<double, std::milli>( ( endSearch - startSearch ) ).count();
         }

         ImGui::Text( "Found %zu matches", searchRes.matchCount );

         const float entryHeight = ImGui::GetTextLineHeightWithSpacing();
         const float totalEntrySize = entryHeight * ( searchRes.tracesIdxThreadIdx.size() + 10 );
         ImGui::SetNextWindowContentSize( ImVec2( 0, totalEntrySize ) );

         ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );

         // Draw the table header
         const auto buttonCol = ImVec4( 0.20f, 0.20f, 0.20f, 0.0f );
         ImGui::PushStyleColor( ImGuiCol_Button, buttonCol );
         ImGui::PushStyleColor( ImGuiCol_ButtonHovered, buttonCol );
         ImGui::PushStyleColor( ImGuiCol_ButtonActive, buttonCol );
         ImGui::BeginChild( "SearchWindow", ImVec2( 0, 0 ), true );
         ImGui::Columns( 3, "SearchResult" );
         if ( ImGui::Button( "Time" ) )
         {
            static bool descending = false;
            descending = !descending;

            if ( descending )
               sortSearchResOnTime( searchRes, tracks, std::greater<TimeStamp>() );
            else
               sortSearchResOnTime( searchRes, tracks, std::less<TimeStamp>() );
         }
         ImGui::NextColumn();
         if ( ImGui::Button( "Trace Name" ) )
         {
            static bool descending = false;
            descending = !descending;

            if ( descending )
               sortSearchResOnName( searchRes, tracks, strDb, std::greater<int>() );
            else
               sortSearchResOnName( searchRes, tracks, strDb, std::less<int>() );
         }
         ImGui::NextColumn();
         if ( ImGui::Button( "Duration" ) )
         {
            static bool descending = false;
            descending = !descending;

            if ( descending )
               sortSearchResOnDuration( searchRes, tracks, std::greater<TimeStamp>() );
            else
               sortSearchResOnDuration( searchRes, tracks, std::less<TimeStamp>() );
         }
         ImGui::NextColumn();
         ImGui::Separator();

         // Find out where to start drawing
         const float curScrollY = ImGui::GetScrollY();
         const size_t startIndex = std::max( ( int64_t )( curScrollY / entryHeight ) - 8ll, 0ll );

         const size_t entryToShow = ImGui::GetWindowHeight() / entryHeight;
         const size_t lastIndex =
             std::min( startIndex + entryToShow + 10, searchRes.tracesIdxThreadIdx.size() );

         // Add dummy invisible button so the next entry show up where they
         // should in the search table
         const float paddingHeight = std::max( curScrollY - ( 3.0f * entryHeight ), 0.1f );
         ImGui::InvisibleButton( "padding1", ImVec2( 1.0f, paddingHeight ) );
         ImGui::NextColumn();
         ImGui::InvisibleButton( "padding2", ImVec2( 1.0f, paddingHeight ) );
         ImGui::NextColumn();
         ImGui::InvisibleButton( "padding3", ImVec2( 1.0f, paddingHeight ) );
         ImGui::NextColumn();

         static size_t selectedId = -1;
         size_t hoveredId = -1;
         char traceTime[64] = {};
         char traceDuration[64] = {};
         bool selectedSomething = false;
         for ( size_t i = startIndex; i < lastIndex; ++i )
         {
            ImGui::PushID( i );
            const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[i];
            const auto& ti              = tracks[traceIdThreadId.second];
            const size_t traceId        = traceIdThreadId.first;
            const TimeStamp start       = ti._traces.entries.starts[traceId];
            const TimeStamp end         = ti._traces.entries.ends[traceId];
            const TimeDuration delta    = end - start;

            hop::formatCyclesTimepointToDisplay(
                start - tlInfo.globalStartTime,
                tlInfo.duration,
                traceTime,
                sizeof( traceTime ),
                tlInfo.useCycles,
                cpuFreqGHz );
            if ( ImGui::Selectable(
                     traceTime, selectedId == i, ImGuiSelectableFlags_SpanAllColumns ) )
            {
               selectedId = i;
               selectedSomething = true;
            }
            if ( ImGui::IsItemHovered() )
            {
               hoveredId = i;
            }
            ImGui::NextColumn();
            ImGui::Text( "%s", strDb.getString( ti._traces.fctNameIds[traceId] ) );
            ImGui::NextColumn();
            hop::formatCyclesDurationToDisplay(
                delta, traceDuration, sizeof( traceDuration ), tlInfo.useCycles, cpuFreqGHz );
            ImGui::Text( "%s", traceDuration );
            ImGui::NextColumn();
            ImGui::PopID();
         }
         ImGui::Columns( 1 );
         ImGui::EndChild();
         ImGui::PopStyleColor( 3 );

         if ( selectedSomething && selectedId != (size_t)-1 )
         {
            const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[selectedId];
            selectedTraceId = traceIdThreadId.first;
            selectedThreadId = traceIdThreadId.second;
         }
         if ( hoveredId != (size_t)-1 )
         {
            const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[hoveredId];
            hoveredTraceId = traceIdThreadId.first;
            hoveredThreadId = traceIdThreadId.second;
         }
         ImGui::PopStyleColor();
      }
      ImGui::End();
      ImGui::PopStyleColor();
   }

   return SearchSelection{selectedTraceId, hoveredTraceId, selectedThreadId, hoveredThreadId};
}

void clearSearchResult( SearchResult& res )
{
   res.stringSearched.clear();
   res.tracesIdxThreadIdx.clear();
   res.matchCount = 0;
}

} // namespace hop