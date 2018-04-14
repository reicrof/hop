#include "Stats.h"
#include "imgui/imgui.h"

namespace hop
{

Stats g_stats = { 0.0, 0.0, 0.0, 0.0, 0, 0 };

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Total Frame time : %f ms\n---------------------", stats.frameTimeMs );
   ImGui::Text( "   Fetching took %f ms\n"
                "   Drawing  took %f ms\n"
                "   Search   took %f ms\n"
                "---------------------", stats.fetchTimeMs, stats.drawingTimeMs, stats.searchTimeMs );
   ImGui::Text("String Db size : %.3f MB", stats.stringDbSize / 1000.0f);
   //ImGui::Text("Total traces size : %zu", stats.traceSize);
   ImGui::Text( "Current LOD : %d", stats.currentLOD );
   ImGui::Text( "Selected trace : %lld", stats.selectedTrace );
}

}