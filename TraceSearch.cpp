#include "TraceSearch.h"
#include "StringDb.h"
#include "ThreadInfo.h"
#include "Utils.h"
#include "imgui/imgui.h"

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
}

void drawSearchResult( const SearchResult& searchRes, const StringDb& strDb, const std::vector< ThreadInfo >& threadInfos, uint64_t totalMicrosInScreen )
{
   ImGui::Text("Found %zu matches", searchRes.matchCount );
   // Draw the table
   ImGui::BeginChild("SearchWindow", ImVec2(0,0), true);
   ImGui::Columns( 3, "SearchResult" );  // 4-ways, with border
   ImGui::Text( "Time" );
   ImGui::NextColumn();
   ImGui::Text( "Trace Name" );
   ImGui::NextColumn();
   ImGui::Text( "Duration" );
   ImGui::NextColumn();
   ImGui::Separator();

   char traceTime[64] = {};
   char traceDuration[64] = {};
   static size_t selected = -1;
   for( size_t i = 0; i < searchRes.tracesIdxThreadIdx.size(); ++i )
   {
      // const ThreadInfo& ti = threadInfos[ i ];
      // for( size_t traceId : searchRes.tracesFoundPerThreadsIdx[i] )
      // {
         const auto& traceIdThreadId = searchRes.tracesIdxThreadIdx[i];
         const auto& ti = threadInfos[ traceIdThreadId.second ];
         const size_t traceId = traceIdThreadId.first;
         const TimeStamp delta = ti.traces.deltas[ traceId ];

         hop::formatMicrosTimepointToDisplay(
             (ti.traces.ends[ traceId ] - delta) * 0.001f,
             totalMicrosInScreen,
             traceTime,
             sizeof( traceTime ) );
         ImGui::Text( "%s", traceTime );
         ImGui::NextColumn();
         ImGui::Text( "%s", strDb.getString( ti.traces.fctNameIds[ traceId ] ) );
         ImGui::NextColumn();
         hop::formatMicrosDurationToDisplay(
             delta * 0.001f,
             traceDuration,
             sizeof( traceDuration ) );
         ImGui::Text( "%s", traceDuration );
         ImGui::NextColumn();
      //}
   }
   ImGui::Columns( 1 );
   ImGui::EndChild();
}

} // namespace hop