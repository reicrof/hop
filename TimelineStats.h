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
      float _zoomFactor;
      double _minRange;
      double _maxRange;
   };
}

#endif //. TIMELINE_STATS_H_