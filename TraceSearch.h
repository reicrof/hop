#ifndef TRACE_SEARCH_H_
#define TRACE_SEARCH_H_

#include <Hop.h>

#include <vector>
#include <string>

namespace hop
{
   class Timeline;
   class TimelineTracks;
   class StringDb;

   struct SearchResult
   {
      std::string stringSearched;
      std::vector< std::pair< size_t, uint32_t> > tracesIdxThreadIdx;
      size_t matchCount{0};
   };

   struct SearchSelection
   {
      size_t selectedTraceIdx;
      size_t hoveredTraceIdx;
      uint32_t selectedThreadIdx;
      uint32_t hoveredThreadIdx;
   };

   void findTraces( const char* string, const StringDb& strDb, const TimelineTracks& tracks, SearchResult& result );
   SearchSelection drawSearchResult( SearchResult& searchRes, TimeStamp globalTimelineStart, TimeDuration timelineDuration, const StringDb& strDb, const TimelineTracks& tracks );
   void clearSearchResult( SearchResult& res );
}

#endif //TRACE_SEARCH_H_