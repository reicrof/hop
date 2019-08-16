#ifndef TRACE_SEARCH_H_
#define TRACE_SEARCH_H_

#include <vector>
#include <string>

namespace hop
{
   class Timeline;
   class TimelineTracks;
   struct TimelineTracksDrawInfo;
   class StringDb;

   struct SearchResult
   {
      std::string stringSearched;
      std::vector< std::pair< size_t, uint32_t> > tracesIdxThreadIdx;
      size_t matchCount{0};
      bool focusSearchWindow{ false };
      bool searchWindowOpen{ false };
   };

   struct SearchSelection
   {
      size_t selectedTraceIdx;
      size_t hoveredTraceIdx;
      uint32_t selectedThreadIdx;
      uint32_t hoveredThreadIdx;
   };

   void findTraces( const char* string, const StringDb& strDb, const TimelineTracks& tracks, SearchResult& result );
   SearchSelection drawSearchResult( SearchResult& searchRes, const TimelineTracksDrawInfo& drawInfo, const TimelineTracks& tracks );
   void clearSearchResult( SearchResult& res );
}

#endif //TRACE_SEARCH_H_