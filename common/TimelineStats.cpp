#include "TimelineStats.h"

namespace hop
{
   void TimelineStats::addStatEventsInt64( const std::vector<StatEvent>& statEvents )
   {
      // Copy the data
      _statEventsInt64.insert( _statEventsInt64.end(), statEvents.begin(), statEvents.end() );
   }

   const std::deque<StatEvent>& TimelineStats::int64Events() const
   {
      return _statEventsInt64;
   }
}