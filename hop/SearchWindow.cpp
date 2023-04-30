#include "hop/SearchWindow.h"
#include "hop/Options.h" // window opacity
#include "hop/TimelineInfo.h"
#include "hop/Stats.h"

#include "common/StringDb.h"
#include "common/TimelineTrack.h"
#include "common/Utils.h"

#include "imgui/imgui.h"

#include <unordered_set>
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

template <typename CMP>
static void sortSearchResOnThread(
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
          return cmp(lhs.second, rhs.second);
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

struct TableSelection
{
   std::unordered_set<int32_t> selections;
   TimeDuration average;
   int32_t lastSelectedId = -1;
   bool dirty;
};

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
      ImVec2 size = ImGui::GetIO().DisplaySize * ImVec2( 0.6f, 0.4f );
      ImVec2 pos = ImGui::GetIO().DisplaySize * ImVec2( 0.5f, 0.5f );
      ImGui::SetNextWindowSize( size, ImGuiCond_Appearing );
      ImGui::SetNextWindowPos( pos, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

      const float wndOpacity = hop::options::windowOpacity();
      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );
      if ( ImGui::Begin( "Search Window", &searchRes.searchWindowOpen ) )
      {
         static char input[512];
         static TableSelection tableSelection;

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
         if( tableSelection.selections.size() > 1 )
         {
            if( tableSelection.dirty )
            {
               tableSelection.average = 0;
               for( int32_t i : tableSelection.selections )
               {
                  const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[i];
                  const size_t traceId        = traceIdThreadId.first;
                  const auto& ti              = tracks[traceIdThreadId.second];
                  tableSelection.average +=
                      ti._traces.entries.ends[traceId] - ti._traces.entries.starts[traceId];
               }
               tableSelection.average /= tableSelection.selections.size();
               tableSelection.dirty = false;
            }
            char buffer[128];
            int cat = snprintf(
                buffer, sizeof( buffer ), "count %zu average ", tableSelection.selections.size() );
            hop::formatCyclesDurationToDisplay(
                tableSelection.average, buffer + cat, sizeof( buffer ) - cat, tlInfo.useCycles, cpuFreqGHz );
            ImVec2 textSize = ImGui::CalcTextSize( buffer );
            ImGui::SameLine( ImGui::GetWindowWidth() - textSize.x - 10.0f );
            ImGui::Text( "%s", buffer );
         }

         // Creating the table modifies the window size, so query values here *
         const float entryHeight    = ImGui::GetTextLineHeightWithSpacing();

         ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.20f, 0.20f, 0.20f, wndOpacity ) );

         uint32_t tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                               ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter;
         if( ImGui::BeginTable( "SearchResult", 4, tableFlags ) )
         {
            const float id_width = ImGui::CalcTextSize("10000000.00 s").x;
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn( "Time", ImGuiTableColumnFlags_WidthFixed, id_width );
            ImGui::TableSetupColumn( "Thread", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Function", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Duration", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            if( ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs() )
            {
                if( sortSpec->SpecsDirty && sortSpec->SpecsCount > 0 )
                {
                   bool descending =
                       sortSpec->Specs[0].SortDirection == ImGuiSortDirection_Descending;
                   switch( sortSpec->Specs[0].ColumnIndex )
                   {
                      case 0: /* Time */
                         if( descending )
                            sortSearchResOnTime( searchRes, tracks, std::greater<TimeStamp>() );
                         else
                            sortSearchResOnTime( searchRes, tracks, std::less<TimeStamp>() );
                         break;
                      case 1: /* Thread */
                         if( descending )
                            sortSearchResOnThread( searchRes, tracks, std::greater<uint32_t>() );
                         else
                            sortSearchResOnThread( searchRes, tracks, std::less<uint32_t>() );
                         break;
                      case 2: /* Name */
                         if( descending )
                            sortSearchResOnName( searchRes, tracks, strDb, std::greater<int>() );
                         else
                            sortSearchResOnName( searchRes, tracks, strDb, std::less<int>() );
                         break;
                      case 3: /* Duration */
                         if( descending )
                            sortSearchResOnDuration( searchRes, tracks, std::greater<TimeStamp>() );
                         else
                            sortSearchResOnDuration( searchRes, tracks, std::less<TimeStamp>() );
                         break;
                   }

                   sortSpec->SpecsDirty = false;
                   tableSelection.selections.clear();
                }
            }

            int doubleClickedId      = -1;
            int hoveredId            = -1;
            char traceTime[64]       = {};
            char traceDuration[64]   = {};

            ImGuiListClipper clipper;
            clipper.Begin( (int)searchRes.tracesIdxThreadIdx.size(), entryHeight );
            while( clipper.Step() )
            {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
                {
                   ImGui::TableNextRow();
                   ImGui::PushID( i );

                   const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[i];
                   const auto& ti              = tracks[traceIdThreadId.second];
                   const size_t traceId        = traceIdThreadId.first;
                   const TimeStamp start       = ti._traces.entries.starts[traceId];
                   const TimeStamp end         = ti._traces.entries.ends[traceId];
                   const TimeDuration delta    = end - start;

                   hop::formatCyclesTimepointToDisplay(
                       start - tlInfo.globalStartTime,
                       5e9 * cpuFreqGHz,
                       traceTime,
                       sizeof( traceTime ),
                       tlInfo.useCycles,
                       cpuFreqGHz );

                   auto& sel = tableSelection.selections;
                   const bool selected = sel.find( i ) != sel.end();
                   ImGui::TableSetColumnIndex( 0 );
                   if( ImGui::Selectable( traceTime, selected, ImGuiSelectableFlags_SpanAllColumns ) )
                   {
                      tableSelection.dirty = true;
                      if( ImGui::GetIO().HopLogicalCtrl )
                      {
                         if( selected )
                            sel.erase( i );
                         else
                            sel.insert( i );
                      }
                      else if( ImGui::GetIO().KeyShift )
                      {
                         if( !sel.empty() )
                         {
                            auto minmax  = std::minmax( tableSelection.lastSelectedId, i );
                            for(int32_t i = minmax.first ; i < minmax.second; i++ )
                               sel.insert( i );
                         }
                         else
                         {
                            sel.insert( i );
                         }
                      }
                      else
                      {
                         sel.clear();
                         sel.insert( i );
                      }
                      tableSelection.lastSelectedId = i;
                   }
                   if( ImGui::IsItemHovered() )
                   {
                      hoveredId = i;
                      if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
                         doubleClickedId = i;
                   }

                   ImGui::TableSetColumnIndex( 1 );
                   if (ti.name ())
                     ImGui::Text("%s", strDb.getString( strDb.getStringIndex( ti.name () ) ));
                   else
                     ImGui::Text("Thread %u", traceIdThreadId.second);

                   ImGui::TableSetColumnIndex( 2 );
                   ImGui::Text( "%s", strDb.getString( ti._traces.fctNameIds[traceId] ) );

                   ImGui::TableSetColumnIndex( 3 );
                   hop::formatCyclesDurationToDisplay(
                       delta,
                       traceDuration,
                       sizeof( traceDuration ),
                       tlInfo.useCycles,
                       cpuFreqGHz );
                   ImGui::Text( "%s", traceDuration );
                   ImGui::PopID();
                }
            }

            if( doubleClickedId != -1 )
            {
                const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[doubleClickedId];
                selectedTraceId             = traceIdThreadId.first;
                selectedThreadId            = traceIdThreadId.second;
            }
            if( hoveredId != -1 )
            {
                const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[hoveredId];
                hoveredTraceId              = traceIdThreadId.first;
                hoveredThreadId             = traceIdThreadId.second;
            }
            ImGui::EndTable();
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
