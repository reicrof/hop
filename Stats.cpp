#include "Stats.h"
#include "imgui/imgui.h"

namespace hop
{

Stats g_stats = {};

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Total Frame time : %f ms\n---------------------", stats.frameTimeMs );
   ImGui::Text( "   Fetching took %f ms\n"
                "   Drawing  took %f ms\n"
                "   Search   took %f ms\n"
                "---------------------", stats.fetchTimeMs, stats.drawingTimeMs, stats.searchTimeMs );
   ImGui::Text( "Current LOD : %d", stats.currentLOD );
   ImGui::Text( "Selected trace : %lld", stats.selectedTrace );
}

}