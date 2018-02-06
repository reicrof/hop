#include "Lod.h"

#include "DisplayableTraces.h"
#include "Utils.h"

#include <algorithm>


static bool canBeLoded(
    int lodLevel,
    vdbg::TimeStamp timeBetweenTrace,
    vdbg::TimeStamp lastTraceDelta,
    vdbg::TimeStamp newTraceDelta )
{
   const auto minTraceSize = vdbg::LOD_MIN_SIZE_MICROS[lodLevel] * 1000;
   const auto maxTimeBetweenTrace = minTraceSize * 1.5;
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
   nonLodedInfos.reserve( src[0].size() );

   // Find the deepest depth
   int deepestDepth = 0;
   for( auto it = src[LOD_COUNT-1].begin(); it != src[LOD_COUNT-1].end(); ++it )
   {
      deepestDepth = std::max( deepestDepth, (int)it->depth );
   }

   // For all LOD levels
   for ( size_t i = 0; i < src.size(); ++i )
   {
      // First trace to insert
      auto newTraceIt = src[i].cbegin();
      size_t sortFromIdx = dst[i].size();

      // If there is already LOD in the dest, try to merge them
      if( dst[i].size() > 0 )
      {
         // For each depth, we go through existing traces (dst) and see if
         // we can merge it with the new trace.
         int depthRemaining = deepestDepth;
         while( depthRemaining >= 0 )
         {
            if( newTraceIt->depth == depthRemaining )
            {
               for( int64_t dstIndex = dst[i].size() - 1; dstIndex >= 0; --dstIndex )
               {
                  // Check if we have found the previous trace at our current depth
                  auto& prevTrace = dst[i][dstIndex];
                  if( prevTrace.depth == depthRemaining )
                  {
                     const auto timeBetweenTrace = (newTraceIt->end - newTraceIt->delta) - prevTrace.end;
                     if( canBeLoded( i, timeBetweenTrace, prevTrace.delta, newTraceIt->delta ) )
                     {
                        prevTrace.end = newTraceIt->end;
                        prevTrace.delta += timeBetweenTrace + newTraceIt->delta;
                        prevTrace.isLoded = true;
                        sortFromIdx = std::min( (size_t)dstIndex, sortFromIdx );
                     }
                     else
                     {
                        nonLodedInfos.push_back( *newTraceIt );
                     }

                     break;
                  }
               }
               --depthRemaining;
            }
            else
            {
               nonLodedInfos.push_back( *newTraceIt );
            }

            // Continue inserting
            ++newTraceIt;
         }
      }

      dst[i].insert( dst[i].end(), nonLodedInfos.begin(), nonLodedInfos.end() );
      dst[i].insert( dst[i].end(), newTraceIt, src[i].end() );

      std::sort( dst[i].begin() + sortFromIdx, dst[i].end() );

      assert_is_sorted( dst[i].begin(), dst[i].end() );

      nonLodedInfos.clear();
   }
}

} // vdbg