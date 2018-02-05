#include "Stats.h"
#include "imgui/imgui.h"

namespace vdbg
{

Stats g_stats = {};

void drawStatsWindow( const Stats& stats )
{
   ImGui::Text( "Frame took %f ms", stats.frameTimeMs );
   ImGui::Text( "Drawing took %f ms", stats.drawingTimeMs );
}

}