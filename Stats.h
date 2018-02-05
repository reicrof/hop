#ifndef STATS_H_
#define STATS_H_

namespace vdbg
{
   struct Stats
   {
      double frameTimeMs;
      double drawingTimeMs;
      double fetchTimeMs;
      int currentLOD;
   };

   extern Stats g_stats;

   void drawStatsWindow( const Stats& stats );
}

#endif //STATS_H_