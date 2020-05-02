#include "hop/Lod.h"

#include "common/TraceData.h"
#include "common/Utils.h"

#include <algorithm>

static constexpr float MIN_TRACE_LENGTH_PXL = 15.0f;
static constexpr float MIN_GAP_PXL = 5.0f;
static hop::hop_timeduration_t LOD_MIN_GAP_CYCLES[hop::LOD_COUNT] = {0};
static hop::hop_timeduration_t LOD_MIN_TRACE_LENGTH_CYCLES[hop::LOD_COUNT] = {0};

static bool canBeLoded(
    int lodLevel,
    hop::hop_timeduration_t timeBetweenTrace,
    hop::hop_timeduration_t lastTraceDelta,
    hop::hop_timeduration_t newTraceDelta )
{
   const hop::hop_timeduration_t minTraceSize = LOD_MIN_TRACE_LENGTH_CYCLES[lodLevel];
   const hop::hop_timeduration_t minTimeBetweenTrace = LOD_MIN_GAP_CYCLES[lodLevel];
   return lastTraceDelta < minTraceSize && newTraceDelta < minTraceSize &&
          timeBetweenTrace < minTimeBetweenTrace;
}

static inline hop::hop_timeduration_t delta( const hop::LodInfo& lod )
{
   return lod.end - lod.start;
}

namespace hop
{
hop_timeduration_t LOD_CYCLES[9] = {1000, 200000, 30000000, 300000000, 600000000, 6000000000, 20000000000, 200000000000, 600000000000 };

void setupLODResolution( uint32_t sreenResolutionX )
{
   for ( uint32_t i = 1; i < LOD_COUNT; ++i )
   {
      LOD_MIN_TRACE_LENGTH_CYCLES[i] =
          pxlToCycles( sreenResolutionX, LOD_CYCLES[i-1], MIN_TRACE_LENGTH_PXL );
      LOD_MIN_GAP_CYCLES[i] = pxlToCycles( sreenResolutionX, LOD_CYCLES[i-1], MIN_GAP_PXL );
   }
}

LodsArray computeLods( const Entries& entries, size_t idOffset )
{
   HOP_ZONE( 2 );
   HOP_PROF_FUNC();
   assert( LOD_MIN_GAP_CYCLES[LOD_COUNT - 1] > 0 && "LOD resolution was not setup" );

   LodsArray resLods;

   std::vector<ssize_t> lastTraceAtDepth( entries.maxDepth + 1, -1 );
   int lodLvl = 0;

   // First LOD is usually never worth merging as they are too small
   {
      HOP_PROF( "Copying first LOD level" );
      for( size_t i = idOffset; i < entries.ends.size(); ++i )
      {
         resLods[0].push_back(
             LodInfo{entries.starts[i], entries.ends[i], i, entries.depths[i], false} );
      }
   }

   // Compute the LOD based on the previous LOD levels
   const hop::Deque<LodInfo>* lastComputedLod = &resLods[lodLvl];
   for ( lodLvl = 1; lodLvl < LOD_COUNT; ++lodLvl )
   {
      HOP_PROF( "Computing next LOD lvl" );

      std::fill( lastTraceAtDepth.begin(), lastTraceAtDepth.end(), -1 );
      
      for ( const auto& l : *lastComputedLod )
      {
         ssize_t lodedTraceIdx = -1;
         const hop_depth_t depth = l.depth;
         if( lastTraceAtDepth[depth] >= 0 )
         {
            const ssize_t lastTraceAtDepthIndex = lastTraceAtDepth[depth];
            auto& lastTrace = resLods[lodLvl][lastTraceAtDepthIndex];
            const hop_timeduration_t timeBetweenTrace = l.start - lastTrace.end;
            if( canBeLoded( lodLvl, timeBetweenTrace, delta( lastTrace ), delta(l) ) )
            {
               assert( lastTrace.depth == depth );
               lodedTraceIdx = lastTraceAtDepthIndex;

               // Update idx since we are about the remove a trace from the list
               for( auto& idx : lastTraceAtDepth )
               {
                  if( idx > lastTraceAtDepthIndex )
                     --idx;
               }

               const hop_timestamp_t newStartTime = lastTrace.start;  // Keep around as it will be
               resLods[lodLvl].erase(
                   resLods[lodLvl].begin() + lastTraceAtDepthIndex,
                   resLods[lodLvl].begin() + lastTraceAtDepthIndex + 1 );

               // New last trace at depth is the next insertion that was merged with
               // the previous last trace
               assert( newStartTime < l.end );
               lastTraceAtDepth[depth] = resLods[lodLvl].size();
               resLods[lodLvl].push_back( LodInfo{newStartTime, l.end, l.index, depth, true} );
            }
         }

         // If it was not loded, insert it and keep index
         if( lodedTraceIdx == -1 )
         {
            // Save idx of the last entry at specific depth
            lastTraceAtDepth[depth] = resLods[lodLvl].size();
            resLods[lodLvl].push_back( LodInfo{l.start, l.end, l.index, l.depth, false} );
         }
      }

      assert_is_sorted( resLods[lodLvl].begin(), resLods[lodLvl].end() );

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

      // If there is already LOD in the dest, try to merge them
      if ( dst.lods[i].size() > 0 )
      {
         // For each depth, we go through existing traces (dst) and see if
         // we can merge it with the new trace.
         int depthRemaining = deepestDepth;
         while ( depthRemaining >= 0 && newTraceIt != src[i].cend() )
         {
            if ( newTraceIt->depth == depthRemaining )
            {
               // Reverse search to find the last trace that has the same depth as the processed one
               auto sameDepthIt = dst.lods[i].end() -1;
               while( sameDepthIt > dst.lods[i].begin() && sameDepthIt->depth != depthRemaining )
               {
                  --sameDepthIt;
               }

               if ( sameDepthIt != dst.lods[i].begin() )
               {
                  const auto timeBetweenTrace = newTraceIt->start - sameDepthIt->end;
                  if ( canBeLoded( i, timeBetweenTrace, delta(*sameDepthIt), delta(*newTraceIt) ) )
                  {
                     newTraceIt->start = sameDepthIt->start;
                     newTraceIt->loded = true;
                     dst.lods[i].erase( sameDepthIt );
                  }
               }

               --depthRemaining;
            }

            // Continue inserting
            ++newTraceIt;
         }
      }

      dst.lods[i].append( src[i].cbegin(), src[i].cend() );
   }

   // Update to the new offset
   dst.idOffset = entries.ends.size();
}

LodsArray
computeCoreEventLods( const Entries& entries, const hop::Deque<hop_core_t>& cores, size_t idOffset )
{
   HOP_PROF_FUNC();

   LodsArray resLods;

   const auto mergeLodInfo = [&cores]( int lodLvl, LodInfo& lastEvent, const LodInfo& newEvent, LodsArray& resLods )
   {
      const int64_t timeBetweenTrace = newEvent.start - lastEvent.end;
      if( cores[lastEvent.index] == cores[newEvent.index] &&
            timeBetweenTrace < LOD_MIN_GAP_CYCLES[lodLvl] )
      {
         lastEvent.end   = newEvent.end;
         lastEvent.loded = true;
      }
      else
      {
         resLods[lodLvl].push_back( newEvent );
      }
   };

   int lodLvl = 0;
   // Compute first LOD from raw data
   {
      // First lod is always added
      resLods[lodLvl].push_back(
          LodInfo{entries.starts[idOffset], entries.ends[idOffset], idOffset, 0, false} );
      HOP_PROF( "Compute first LOD level" );
      for ( size_t i = idOffset + 1; i < entries.ends.size(); ++i )
      {
         mergeLodInfo(
            lodLvl,
            resLods[lodLvl].back(),
            LodInfo{entries.starts[i], entries.ends[i], i, 0, false},
            resLods );
      }
      std::sort( resLods[lodLvl].begin(), resLods[lodLvl].end() );
   }

   // Compute the LOD based on the previous LOD levels
   const hop::Deque<LodInfo>* lastComputedLod = &resLods[lodLvl];
   for ( lodLvl = 1; lodLvl < LOD_COUNT; ++lodLvl )
   {
      HOP_PROF( "Computing next LOD lvl" );
      resLods[lodLvl].push_back( lastComputedLod->front() );
      for ( const auto& l : *lastComputedLod )
      {
         mergeLodInfo( lodLvl, resLods[lodLvl].back(), l, resLods );
      }
      std::sort( resLods[lodLvl].begin(), resLods[lodLvl].end() );

      // Update the last compute lod ptr
      lastComputedLod = &resLods[lodLvl];
   }

   return resLods;
}

void appendCoreEventLods( LodsData& dst, const Entries& entries, const hop::Deque<hop_core_t>& cores )
{
   if( entries.ends.size() <= dst.idOffset ) return;

   LodsArray src = computeCoreEventLods( entries, cores, dst.idOffset );

   HOP_PROF_FUNC();

   // For all LOD levels
   for( size_t lodLvl = 0; lodLvl < src.size(); ++lodLvl )
   {
      // First trace to insert
      const auto& newEvent  = src[lodLvl].front();
      long sortFromIdx      = dst.lods[lodLvl].size();

      // If there is already LOD in the dest, try to merge them
      bool loded = false;
      if( !dst.lods[lodLvl].empty() )
      {
         auto& prevEvent                = dst.lods[lodLvl].back();
         const int64_t timeBetweenTrace = newEvent.start - prevEvent.end;
         if( cores[prevEvent.index] == cores[newEvent.index] &&
             timeBetweenTrace < LOD_MIN_GAP_CYCLES[lodLvl] )
         {
            prevEvent.end   = newEvent.end;
            prevEvent.loded = loded = true;
         }
      }

      // Insert all the events except the first one if it has been loded
      dst.lods[lodLvl].append( src[lodLvl].cbegin() + (int)loded, src[lodLvl].cend() );
      if( loded )
      {
         std::sort(
             dst.lods[lodLvl].begin() + std::max( 0l, ( sortFromIdx - 1 ) ),
             dst.lods[lodLvl].end() );
      }

      assert_is_sorted( dst.lods[lodLvl].begin(), dst.lods[lodLvl].end() );
   }

   // Update to the new offset
   dst.idOffset = entries.ends.size();
}

std::pair<size_t, size_t> visibleIndexSpan(
    const LodsArray& lodsArr,
    int lodLvl,
    hop_timestamp_t absoluteStart,
    hop_timestamp_t absoluteEnd,
    hop_depth_t lowestDepth )
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