#ifndef TIMELINE_STATS_H_
#define TIMELINE_STATS_H_

#include "TimelineInfo.h"
#include "Hop.h"

#include <deque>
#include <vector>

namespace hop
{
   class TimelineStats
   {
   public:
      std::vector< TimelineMessage > draw();
      void addStatEvents( const std::vector<StatEvent>& statEvents );
   private:
      std::deque<StatEvent> _statEvents;
   };
}

#endif //. TIMELINE_STATS_H_