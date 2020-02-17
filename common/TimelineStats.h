#ifndef TIMELINE_STATS_H_
#define TIMELINE_STATS_H_

#include "Hop.h"

#include <deque>
#include <vector>

namespace hop
{
   class TimelineStats
   {
   public:
      void addStatEventsInt64( const std::vector<StatEvent>& statEvents );
      const std::deque<StatEvent>& int64Events() const;
   private:
      std::deque<StatEvent> _statEventsInt64;
   };
}

#endif // TIMELINE_STATS_H_
