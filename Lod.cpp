#include "Lod.h"

#include "DisplayableTraces.h"

#include <algorithm>
#include <cassert>

namespace vdbg
{
std::array<std::vector<LodInfo>, LOD_COUNT> computeLods( const DisplayableTraces& traces )
{
   std::array<std::vector<LodInfo>, LOD_COUNT> resLods;
   for ( auto& lodInfo : resLods ) lodInfo.reserve( 256 );

   // Compute LODs.
   TDepth_t maxDepth = *std::max_element( traces.depths.begin(), traces.depths.end() );
   std::vector<std::vector<LodInfo> > lods( maxDepth + 1 );

   // Compute first LOD from raw data
   int lodLvl = 0;
   for ( size_t i = 0; i < traces.ends.size(); ++i )
   {
      const TDepth_t curDepth = traces.depths[i];
      if ( lods[curDepth].empty() )
      {
         lods[curDepth].push_back( LodInfo{traces.ends[i], traces.deltas[i], i, curDepth, false} );
         continue;
      }

      auto& lastTrace = lods[curDepth].back();
      const auto timeBetweenTrace = ( traces.ends[i] - traces.deltas[i] ) - lastTrace.end;
      const auto minTraceSize = LOD_MIN_SIZE_MICROS[lodLvl] * 1000;
      const auto maxTimeBetweenTrace = minTraceSize;
      if ( lastTrace.delta < minTraceSize && traces.deltas[i] < minTraceSize &&
           timeBetweenTrace < maxTimeBetweenTrace )
      {
         assert( lastTrace.depth == curDepth );
         lastTrace.end = traces.ends[i];
         lastTrace.delta += timeBetweenTrace + traces.deltas[i];
         lastTrace.isLoded = true;
      }
      else
      {
         lods[curDepth].push_back( LodInfo{traces.ends[i], traces.deltas[i], i, curDepth, false} );
      }
   }

   for ( const auto& l : lods )
   {
      resLods[lodLvl].insert( resLods[lodLvl].end(), l.begin(), l.end() );
   }
   std::sort( resLods[lodLvl].begin(), resLods[lodLvl].end() );

   // Clear depth lods to reuse them for next lods
   for ( auto& l : lods )
   {
      l.clear();
   }

   // Compute the LOD based on the previous LOD levels
   const std::vector<LodInfo>* lastComputedLod = &resLods[lodLvl];
   for ( lodLvl = 1; lodLvl < LOD_COUNT; ++lodLvl )
   {
      for ( const auto& l : *lastComputedLod )
      {
         const TDepth_t curDepth = l.depth;
         if ( lods[curDepth].empty() )
         {
            lods[curDepth].emplace_back( l );
            continue;
         }

         auto& lastTrace = lods[curDepth].back();
         const auto timeBetweenTrace = ( l.end - l.delta ) - lastTrace.end;
         const auto minTraceSize = LOD_MIN_SIZE_MICROS[lodLvl] * 1000;
         const auto maxTimeBetweenTrace = 2 * minTraceSize;
         if ( lastTrace.delta < minTraceSize && l.delta < minTraceSize &&
              timeBetweenTrace < maxTimeBetweenTrace )
         {
            assert( lastTrace.depth == curDepth );
            lastTrace.end = l.end;
            lastTrace.delta += timeBetweenTrace + l.delta;
            lastTrace.isLoded = true;
         }
         else
         {
            lods[curDepth].emplace_back( l );
         }
      }

      for ( const auto& l : lods )
      {
         resLods[lodLvl].insert( resLods[lodLvl].end(), l.begin(), l.end() );
      }
      std::sort( resLods[lodLvl].begin(), resLods[lodLvl].end() );

      // Clear for reuse
      for ( auto& l : lods ) l.clear();

      // Update the last compute lod ptr
      lastComputedLod = &resLods[lodLvl];
   }

   return resLods;
}
}