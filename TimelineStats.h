#ifndef TIMELINE_STATS_H_
#define TIMELINE_STATS_H_

#include "TimelineInfo.h"

#include <vector>

namespace hop
{
   class TimelineStats
   {
   public:
      std::vector< TimelineMessage > draw();
   };
}

#endif //. TIMELINE_STATS_H_