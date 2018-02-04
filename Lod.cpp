#include "Lod.h"

#include "DisplayableTraces.h"

#include <algorithm>
#include <cassert>

static bool canBeLoded(
    int lodLevel,
    vdbg::TimeStamp timeBetweenTrace,
    vdbg::TimeStamp lastTraceDelta,
    vdbg::TimeStamp newTraceDelta )
{
   const auto minTraceSize = vdbg::LOD_MIN_SIZE_MICROS[lodLevel] * 1000;
   const auto maxTimeBetweenTrace = minTraceSize * 2;
   return lastTraceDelta < minTraceSize && newTraceDelta < minTraceSize &&
        timeBetweenTrace < maxTimeBetweenTrace;
}

namespace vdbg
{
LodsArray computeLods( const DisplayableTraces& traces, size_t idOffset )
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
         lods[curDepth].push_back( LodInfo{traces.ends[i], traces.deltas[i], idOffset + i, curDepth, false} );
         continue;
      }

      auto& lastTrace = lods[curDepth].back();
      const auto timeBetweenTrace = (traces.ends[i] - traces.deltas[i]) - lastTrace.end;
      if( canBeLoded( lodLvl, timeBetweenTrace, lastTrace.delta, traces.deltas[i] ) )
      {
         assert( lastTrace.depth == curDepth );
         lastTrace.end = traces.ends[i];
         lastTrace.delta += timeBetweenTrace + traces.deltas[i];
         lastTrace.isLoded = true;
      }
      else
      {
         lods[curDepth].push_back( LodInfo{traces.ends[i], traces.deltas[i], idOffset + i, curDepth, false} );
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
         const auto timeBetweenTrace = (l.end - l.delta) - lastTrace.end;
         if( canBeLoded( lodLvl, timeBetweenTrace, lastTrace.delta, l.delta ) )
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

void appendLods( LodsArray& dst, const LodsArray& src )
{
   std::vector< LodInfo > nonLodedInfos;
   nonLodedInfos.reserve( dst[0].size() );

   // For all LOD levels
   for ( size_t i = 0; i < src.size(); ++i )
   {
      auto newTraceIt = src[i].cbegin();
      // If there is already LOD in the dest, try to merge them
      if( dst[i].size() > 0 )
      {
         int deepestDepth = 0;
         for( auto it = src[i].begin(); it != src[i].end(); ++it )
         {
            if( it->depth == 0 ) break;
            deepestDepth = std::max( deepestDepth, (int)it->depth );
         }

         while( deepestDepth >= 0 )
         {
            if( newTraceIt->depth == deepestDepth )
            {
               for( auto it = dst[i].rbegin(); it != dst[i].rend(); ++it )
               {
                  // We have found the previous trace at our current depth
                  if( it->depth == deepestDepth )
                  {
                     const auto timeBetweenTrace = (newTraceIt->end - newTraceIt->delta) - it->end;
                     if( canBeLoded( i, timeBetweenTrace, it->delta, newTraceIt->delta ) )
                     {
                        it->end = newTraceIt->end;
                        it->delta += timeBetweenTrace + newTraceIt->delta;
                        it->isLoded = true;
                     }
                     else
                     {
                        nonLodedInfos.push_back( *newTraceIt );
                     }

                     break;
                  }
               }
               --deepestDepth;
            }
            else
            {
               nonLodedInfos.push_back( *newTraceIt );
            }

            // Continue inserting
            ++newTraceIt;
         }
      }

      const size_t prevSize = dst[i].size();
      dst[i].insert( dst[i].end(), nonLodedInfos.begin(), nonLodedInfos.end() );
      dst[i].insert( dst[i].end(), newTraceIt, src[i].end() );

      nonLodedInfos.clear();

      std::sort( dst[i].begin(), dst[i].end() );
      //assert( std::is_sorted( dst[i].begin(), dst[i].end() ) );
   }
}

} // vdbg