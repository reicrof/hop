#include "Lod.h"

#include "DisplayableTraces.h"
#include "Utils.h"

#include <algorithm>

static constexpr float MIN_TRACE_LENGTH_PXL = 3.0f;
static constexpr float MIN_GAP_PXL = 2.0f;
static hop::TimeDuration LOD_MIN_GAP_PXL[hop::LOD_COUNT] = {0};
static hop::TimeDuration LOD_MIN_TRACE_LENGTH_PXL[hop::LOD_COUNT] = {0};

static bool canBeLoded(
    int lodLevel,
    hop::TimeDuration timeBetweenTrace,
    hop::TimeDuration lastTraceDelta,
    hop::TimeDuration newTraceDelta )
{
   const hop::TimeDuration minTraceSize = LOD_MIN_TRACE_LENGTH_PXL[lodLevel];
   const hop::TimeDuration minTimeBetweenTrace = LOD_MIN_GAP_PXL[lodLevel];
   return lastTraceDelta < minTraceSize && newTraceDelta < minTraceSize &&
        timeBetweenTrace < minTimeBetweenTrace;
}

namespace hop
{
   
void setupLODResolution( uint32_t sreenResolutionX )
{
   for( uint32_t i = 0; i < LOD_COUNT; ++i )
   {
      LOD_MIN_TRACE_LENGTH_PXL[i] = pxlToNanos( sreenResolutionX, LOD_NANOS[i], MIN_TRACE_LENGTH_PXL );
      LOD_MIN_GAP_PXL[i] = pxlToNanos( sreenResolutionX, LOD_NANOS[i], MIN_GAP_PXL );
   }
}

LodsArray computeLods( const DisplayableTraces& traces, size_t idOffset )
{
   assert( LOD_MIN_GAP_PXL[LOD_COUNT-1] > 0 && "LOD resolution was not setup" );

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
      const TimeDuration timeBetweenTrace = (traces.ends[i] - traces.deltas[i]) - lastTrace.end;
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
      long sortFromIdx = dst[i].size();

      // If there is already LOD in the dest, try to merge them
      if( dst[i].size() > 0 )
      {
         // For each depth, we go through existing traces (dst) and see if
         // we can merge it with the new trace.
         int depthRemaining = deepestDepth;
         while( depthRemaining >= 0 && newTraceIt != src[i].cend() )
         {
            bool wasLoded = false;
            if( newTraceIt->depth == depthRemaining )
            {
               // Find the last trace that has the same depth as the processed one
               const auto sameDepthIt = std::find_if( dst[i].rbegin(), dst[i].rend(), [=]( const LodInfo& o )
               {
                  return o.depth == depthRemaining;
               } );

               // Check if we can merge it if the previous one at the same depth
               if( sameDepthIt != dst[i].rend() )
               {
                  const auto timeBetweenTrace = (newTraceIt->end - newTraceIt->delta) - sameDepthIt->end;
                  wasLoded = canBeLoded( i, timeBetweenTrace, sameDepthIt->delta, newTraceIt->delta );
                  if( wasLoded )
                  {
                     sameDepthIt->end = newTraceIt->end;
                     sameDepthIt->delta += timeBetweenTrace + newTraceIt->delta;
                     sameDepthIt->isLoded = true;
                     const long dist = std::distance( sameDepthIt, dst[i].rend());
                     sortFromIdx = std::min( dist, sortFromIdx );
                  }
               }

               --depthRemaining;
            }

            if( !wasLoded )
               nonLodedInfos.push_back( *newTraceIt );

            // Continue inserting
            ++newTraceIt;
         }
      }

      dst[i].insert( dst[i].end(), nonLodedInfos.begin(), nonLodedInfos.end() );
      dst[i].insert( dst[i].end(), newTraceIt, src[i].end() );

      std::sort( dst[i].begin() + std::max( 0l, (sortFromIdx-1) ), dst[i].end() );

      assert_is_sorted( dst[i].begin(), dst[i].end() );

      nonLodedInfos.clear();
   }
}

} // namespace hop