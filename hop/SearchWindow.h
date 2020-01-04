#ifndef HOP_SEARCH_WINDOW_H_
#define HOP_SEARCH_WINDOW_H_

#include <vector>
#include <string>

namespace hop
{
   class StringDb;
   struct TimelineInfo;
   struct TimelineTrack;

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

   void findTraces( const char* string, const StringDb& strDb, const std::vector<TimelineTrack>& tracks, SearchResult& result );
   SearchSelection drawSearchResult( SearchResult& searchRes, const StringDb& strDb, const TimelineInfo& tlInfo, const std::vector<TimelineTrack>& tracks );
   void clearSearchResult( SearchResult& res );
}

#endif // HOP_SEARCH_WINDOW_H_