#include "TimelineStats.h"

namespace hop
{
   std::vector< TimelineMessage > TimelineStats::draw()
   {
      std::vector< TimelineMessage > messages;

      return messages;
   }

   void TimelineStats::addStatEvents( const std::vector<StatEvent>& statEvents )
   {
      _statEvents.insert( _statEvents.end(), statEvents.begin(), statEvents.end() );
   }
}