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
      std::vector< TimelineMessage > draw( const TimelineDrawInfo& );
      void addStatEvents( const std::vector<StatEvent>& statEvents );
   private:
      std::deque<StatEvent> _statEvents;
      float _zoomFactor;
      double _minRange;
      double _maxRange;
   };
}

#endif //. TIMELINE_STATS_H_