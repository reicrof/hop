#include "Stats.h"
#include "imgui/imgui.h"

namespace vdbg
{

Stats g_stats = {};

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Total Frame time : %f ms\n---------------------", stats.frameTimeMs );
   ImGui::Text( "   Fetching took %f ms\n"
                "   Drawing took %f ms\n"
                "---------------------", stats.fetchTimeMs, stats.drawingTimeMs );
   ImGui::Text( "Current LOD : %d", stats.currentLOD );
}

}