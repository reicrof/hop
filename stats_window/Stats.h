#ifndef STATS_H_
#define STATS_H_

#include <cstddef>

namespace hop
{
   struct Stats
   {
      double frameTimeMs;
      double drawingTimeMs;
      double traceDrawingTimeMs;
      double lockwaitsDrawingTimeMs;
      double coreDrawingTimeMs;
      double fetchTimeMs;
      double searchTimeMs;
      int currentLOD;
      size_t stringDbSize;
      size_t traceCount;
      size_t clientSharedMemSize;
      float  canvasScrollAmnt;
   };

   extern Stats g_stats;

   void drawStatsWindow( const Stats& stats );
}

#endif //STATS_H_