#include "Stats.h"
#include "Utils.h"
#include "imgui/imgui.h"

namespace hop
{

Stats g_stats = { 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0 };

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Total Frame time : %f ms\n---------------------", stats.frameTimeMs );
   ImGui::Text( "   Fetching took %f ms\n"
                "   Drawing  took %f ms\n"
                "   Search   took %f ms\n"
                "---------------------", stats.fetchTimeMs, stats.drawingTimeMs, stats.searchTimeMs );

   char formatStr[32];
   formatSizeInBytesToDisplay( stats.stringDbSize, formatStr, sizeof(formatStr) );
   ImGui::Text("String Db size : %s", formatStr);
   //ImGui::Text("Total traces size : %zu", stats.traceSize);
   ImGui::Text("Traces count : %zu", stats.traceCount);
   ImGui::Text( "Current LOD : %d", stats.currentLOD );
}

}