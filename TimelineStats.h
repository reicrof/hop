#ifndef TIMELINE_STATS_H_
#define TIMELINE_STATS_H_

#include "TimelineInfo.h"
#include "Hop.h"

#include <deque>
#include <vector>

namespace hop
{
   class StringDb;
   struct TimelineInfo;
   class TimelineStats
   {
   public:
      TimelineStats();
      float canvasHeight() const;
      void draw( const TimelineDrawInfo&, TimelineMsgArray& outMessages );
      void addStatEventsInt64( const std::vector<StatEvent>& statEvents );
      void addStatEventsFloat64( const std::vector<StatEvent>& statEvents );
   private:
      std::deque<StatEvent> _statEventsInt64;
      std::deque<StatEvent> _statEventsFloat64;
      int64_t _minMaxValueInt64[4]; // [0] = min, [1] = max, [2] = last min, [3] = last max
      double _minMaxValueDbl[4];    // [0] = min, [1] = max, [2] = last min, [3] = last max
      float _zoomFactor;
   };
}

#endif // TIMELINE_STATS_H_
