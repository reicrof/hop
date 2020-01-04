#include "hop/Lod.h"

#include "common/TraceData.h"
#include "common/Utils.h"

#include <algorithm>

static constexpr float MIN_TRACE_LENGTH_PXL = 10.0f;
static constexpr float MIN_GAP_PXL = 5.0f;
static hop::TimeStamp LOD_MIN_GAP_CYCLES[hop::LOD_COUNT] = {0};
static hop::TimeStamp LOD_MIN_TRACE_LENGTH_PXL[hop::LOD_COUNT] = {0};

#define HOP_USE_INSERTION_SORT 0

static bool canBeLoded(
    int lodLevel,
    hop::TimeDuration timeBetweenTrace,
    hop::TimeDuration lastTraceDelta,
    hop::TimeDuration newTraceDelta )
{
   const hop::TimeDuration minTraceSize = LOD_MIN_TRACE_LENGTH_PXL[lodLevel];
   const hop::TimeDuration minTimeBetweenTrace = LOD_MIN_GAP_CYCLES[lodLevel];
   return lastTraceDelta < minTraceSize && newTraceDelta < minTraceSize &&
          timeBetweenTrace < minTimeBetweenTrace;
}

static inline hop::TimeDuration delta( const hop::LodInfo& lod )
{
   return lod.end - lod.start;
}

namespace hop
{
TimeDuration LOD_CYCLES[9] = {1000, 200000, 30000000, 300000000, 600000000, 6000000000, 20000000000, 200000000000, 600000000000 };

void setupLODResolution( uint32_t sreenResolutionX )
{
   for ( uint32_t i = 1; i < LOD_COUNT; ++i )
   {
      LOD_MIN_TRACE_LENGTH_PXL[i] =
          pxlToCycles( sreenResolutionX, LOD_CYCLES[i-1], MIN_TRACE_LENGTH_PXL );
      LOD_MIN_GAP_CYCLES[i] = pxlToCycles( sreenResolutionX, LOD_CYCLES[i-1], MIN_GAP_PXL );
   }
}

LodsArray computeLods( const Entries& entries, size_t idOffset )
{
   HOP_PROF_FUNC();
   assert( LOD_MIN_GAP_CYCLES[LOD_COUNT - 1] > 0 && "LOD resolution was not setup" );

   LodsArray resLods;

   // Compute LODs.
   std::vector<std::vector<LodInfo> > lods( entries.maxDepth + 1 );
   int lodLvl = 0;

   // Compute first LOD from raw data
   {
      HOP_PROF( "Compute first LOD level" );
      for ( size_t i = idOffset; i < entries.ends.size(); ++i )
      {
         const Depth_t curDepth = entries.depths[i];
         if ( lods[curDepth].empty() )
         {
            lods[curDepth].push_back(
                LodInfo{entries.starts[i], entries.ends[i], i, curDepth, false} );
            continue;
         }

         auto& lastTrace = lods[curDepth].back();
         const TimeDuration timeBetweenTrace = entries.starts[i] - lastTrace.end;
         if( canBeLoded(
                 lodLvl,
                 timeBetweenTrace,
                 delta( lastTrace ),
                 entries.ends[i] - entries.starts[i] ) )
         {
            assert( lastTrace.depth == curDepth );
            lastTrace.end = entries.ends[i];
            lastTrace.loded = true;
         }
         else
         {
            lods[curDepth].push_back(
                LodInfo{entries.starts[i], entries.ends[i], i, curDepth, false} );
         }
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
   const std::deque<LodInfo>* lastComputedLod = &resLods[lodLvl];
   for ( lodLvl = 1; lodLvl < LOD_COUNT; ++lodLvl )
   {
      HOP_PROF( "Computing next LOD lvl" );
      for ( const auto& l : *lastComputedLod )
      {
         const Depth_t curDepth = l.depth;
         if ( lods[curDepth].empty() )
         {
            lods[curDepth].emplace_back( l );
            continue;
         }

         auto& lastTrace = lods[curDepth].back();
         const auto timeBetweenTrace = l.start - lastTrace.end;
         if ( canBeLoded( lodLvl, timeBetweenTrace, delta(lastTrace), delta(l) ) )
         {
            assert( lastTrace.depth == curDepth );
            lastTrace.end   = l.end;
            lastTrace.loded = true;
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

void appendLods( LodsData& dst, const Entries& entries )
{
   if( entries.ends.size() <= dst.idOffset ) return;

   LodsArray src = computeLods( entries, dst.idOffset );

   HOP_PROF_FUNC();
   std::vector<LodInfo> nonLodedInfos;
   nonLodedInfos.reserve( src[0].size() );

   // Find the deepest depth
   int deepestDepth = 0;
   for ( auto it = src[LOD_COUNT - 1].begin(); it != src[LOD_COUNT - 1].end(); ++it )
   {
      deepestDepth = std::max( deepestDepth, (int)it->depth );
   }

   // For all LOD levels
   for ( size_t i = 0; i < src.size(); ++i )
   {
      // First trace to insert
      auto newTraceIt = src[i].cbegin();
      long sortFromIdx = dst.lods[i].size();

      // If there is already LOD in the dest, try to merge them
      if ( dst.lods[i].size() > 0 )
      {
         // For each depth, we go through existing traces (dst) and see if
         // we can merge it with the new trace.
         int depthRemaining = deepestDepth;
         while ( depthRemaining >= 0 && newTraceIt != src[i].cend() )
         {
            bool wasLoded = false;
            if ( newTraceIt->depth == depthRemaining )
            {
               // Find the last trace that has the same depth as the processed one
               const auto sameDepthIt =
                   std::find_if( dst.lods[i].rbegin(), dst.lods[i].rend(), [=]( const LodInfo& o ) {
                      return o.depth == depthRemaining;
                   } );

               // Check if we can merge it if the previous one at the same depth
               if ( sameDepthIt != dst.lods[i].rend() )
               {
                  const auto timeBetweenTrace = newTraceIt->start - sameDepthIt->end;
                  wasLoded = 
                     canBeLoded( i, timeBetweenTrace, delta(*sameDepthIt), delta(*newTraceIt) );
                  if ( wasLoded )
                  {
                     sameDepthIt->end   = newTraceIt->end;
                     sameDepthIt->loded = true;
                     const long dist    = std::distance( sameDepthIt, dst.lods[i].rend() );
                     sortFromIdx        = std::min( dist, sortFromIdx );
                  }
               }

               --depthRemaining;
            }

            if ( !wasLoded ) nonLodedInfos.push_back( *newTraceIt );

            // Continue inserting
            ++newTraceIt;
         }
      }

      dst.lods[i].insert( dst.lods[i].end(), nonLodedInfos.begin(), nonLodedInfos.end() );
      dst.lods[i].insert( dst.lods[i].end(), newTraceIt, src[i].cend() );

      std::sort( dst.lods[i].begin() + std::max( 0l, ( sortFromIdx - 1 ) ), dst.lods[i].end() );

      assert_is_sorted( dst.lods[i].begin(), dst.lods[i].end() );

      nonLodedInfos.clear();
   }

   // Update to the new offset
   dst.idOffset = entries.ends.size();
}

std::pair<size_t, size_t> visibleIndexSpan(
    const LodsArray& lodsArr,
    int lodLvl,
    TimeStamp absoluteStart,
    TimeStamp absoluteEnd,
    Depth_t lowestDepth )
{
   auto span = std::make_pair( hop::INVALID_IDX, hop::INVALID_IDX );

   const auto& lods = lodsArr[lodLvl];
   const LodInfo firstInfo = {absoluteStart, absoluteStart, 0, 0, false};
   const LodInfo lastInfo = {absoluteEnd, absoluteEnd, 0, 0, false};
   auto it1 = std::lower_bound( lods.begin(), lods.end(), firstInfo );
   auto it2 = std::upper_bound( lods.begin(), lods.end(), lastInfo );

   // The last trace of the current thread does not reach the current time
   if ( it1 == lods.end() ) return span;

   // Find the the first trace on right that have a depth of "baseDepth". This can be either 0
   // for traces or 1 for lockwaits. This prevents traces that have a smaller depth than the
   // one foune previously to vanish.
   while ( it2 != lods.end() && it2->depth > lowestDepth )
   {
      ++it2;
   }
   // We need to go 1 past the trace with a depth of 0
   if( it2 != lods.end() ) ++it2;

   span.first = std::distance( lods.begin(), it1 );
   span.second = std::distance( lods.begin(), it2 );
   return span;
}

}  // namespace hop