#ifndef TIMELINE_STATS_VIEW_H_
#define TIMELINE_STATS_VIEW_H_

#include "Hop.h"
#include "common/TimelineStats.h"

#include <deque>
#include <vector>

namespace hop
{
   class Profiler;
   class TimelineMsgArray;
   struct TimelineInfo;

   // External data coming from the profiler and the timeline
   struct TimelineStatsDrawData
   {
      const Profiler& profiler;
      const TimelineInfo& timeline;
      const int lodLevel;
      const float highlightValue;
   };

   class TimelineStatsView
   {
   public:
      TimelineStatsView();
      float canvasHeight() const;
      void draw( const TimelineStatsDrawData&, TimelineMsgArray* outMessages );
      void addStatEventsInt64( const std::vector<StatEvent>& statEvents );

      static float originScrollAmount( float windowHeight );
   private:
      TimelineStats _timelineStats;
      int64_t _minMaxValueInt64[2];
      double _minMaxValueDbl[2];
      float _zoomFactor;         // Value per pxl
      double _valueSteps;        // Grid step size
   };
}

#endif // TIMELINE_STATS_VIEW_H_
