#include "Stats.h"
#include "Utils.h"
#include "imgui/imgui.h"

namespace hop
{

Stats g_stats = { 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0 };

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Total Frame time : %f ms\n---------------------", stats.frameTimeMs );
   ImGui::Text( "   Fetching took %f ms\n"
                "   Drawing  took %f ms\n"
                "      Traces     %f ms\n"
                "      LockWaits  %f ms\n"
                "      Cores      %f ms\n"
                "   Search   took %f ms\n"
                "---------------------",
                stats.fetchTimeMs,
                stats.drawingTimeMs,
                stats.traceDrawingTimeMs,
                stats.lockwaitsDrawingTimeMs,
                stats.coreDrawingTimeMs,
                stats.searchTimeMs );
   char formatStr[32];
   formatSizeInBytesToDisplay( stats.clientSharedMemSize, formatStr, sizeof(formatStr) );
   ImGui::Text( "Shared Memory Size : %s\n", formatStr );

   formatSizeInBytesToDisplay( stats.stringDbSize, formatStr, sizeof(formatStr) );
   ImGui::Text("String Db size : %s", formatStr);
   ImGui::Text("Traces count : %zu", stats.traceCount);
   ImGui::Text("Current LOD : %d", stats.currentLOD);
}

}