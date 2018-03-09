#ifndef TRACE_SEARCH_H_
#define TRACE_SEARCH_H_

#include <vector>
#include <string>

namespace hop
{
   class Timeline;
   class StringDb;
   struct ThreadInfo;

   struct SearchResult
   {
      std::string stringSearched;
      std::vector< std::pair< size_t, uint32_t> > tracesIdxThreadIdx;
      size_t matchCount{0};
   };

   void findTraces( const char* string, const StringDb& strDb, const std::vector< ThreadInfo >& threadInfos, SearchResult& result );
   std::pair< size_t, uint32_t > drawSearchResult( SearchResult& searchRes, const Timeline& timeline, const StringDb& strDb, const std::vector< ThreadInfo >& threadInfos );
}

#endif //TRACE_SEARCH_H_