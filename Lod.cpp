#include "Lod.h"

#include "common/TraceData.h"
#include "common/Utils.h"

#include <algorithm>

static constexpr float MIN_TRACE_LENGTH_PXL = 10.0f;
static constexpr float MIN_GAP_PXL = 5.0f;
static hop::TimeStamp LOD_MIN_GAP_CYCLES[hop::LOD_COUNT] = {0};
static hop::TimeStamp LOD_MIN_TRACE_LENGTH_PXL[hop::LOD_COUNT] = {0};

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

template< unsigned LODLVL >
static hop::LodInfo createLod( size_t index, hop::TimeStamp start, hop::TimeStamp end, hop::TimeDuration delta, const hop::LodInfo& prevLod, LodsArray& resultLods )
{
   const TimeDuration lastTraceDelta       = prevLod.end - prevLod.start;
   const auto minMaxValue                  = std::minmax( start, prevLod.end );
   const TimeStamp timeBetweenTrace        = minMaxValue.second - minMaxValue.first;

   const hop::TimeDuration minTraceSize   = LOD_MIN_TRACE_LENGTH_PXL[LODLVL];
   const bool lastTraceSmallEnough        = lastTraceDelta < minTraceSize;
   const bool newTraceSmallEnough         = delta  < minTraceSize;
   const bool timeBetweenTraceSmallEnough = timeBetweenTrace < LOD_MIN_GAP_CYCLES[LODLVL];

   LodInfo computedLod = prevLod;
   if( lastTraceSmallEnough && newTraceSmallEnough && timeBetweenTraceSmallEnough )
   {
      computedLod.loded = true;
      assert( computedLod.index != hop::INVALID_IDX );
   }
   else
   {
      if( computedLod.index != hop::INVALID_IDX ) {
         resultLods[LODLVL].push_back( computedLod );
      }

      computedLod.start = start;
      computedLod.index = index;
      computedLod.loded = false;
   }

   computedLod.end = end;

   return computedLod;
}

// Adds missing 'latest' entry for new depths and returns the end timestamp of the earliest modifiable lod
static std::array<hop::TimeStamp, LOD_COUNT>
initializeLodsPerDepth( LodsData& lodData, const Entries& entries )
{
   HOP_PROF_FUNC();
   const size_t curDepth = lodData.latestLodPerDepth.size();

   // Find the earliest possible lodinfo that could be modified
   const size_t earliestIndex = curDepth == 0 ? 0 : hop::INVALID_IDX;
   std::array<TimeStamp, LOD_COUNT> earliestEnd;
   std::fill( earliestEnd.begin(), earliestEnd.end(), earliestIndex );
   for( unsigned i = 0; i < curDepth; ++i )
   {
      for( unsigned j = 0; j < LOD_COUNT; ++j )
      {
         earliestEnd[j] = std::min( earliestEnd[j], lodData.latestLodPerDepth[i][j].end );
      }
   }

   // Then add the missing lodinfo for the new 'depths'
   const size_t maxDepth = entries.maxDepth + 1;
   assert(maxDepth >= curDepth);
   for( size_t i = 0; i < maxDepth - curDepth; ++i )
   {
      Depth_t depth = curDepth + i;
      lodData.latestLodPerDepth.emplace_back();
      std::fill( lodData.latestLodPerDepth.back().begin(), lodData.latestLodPerDepth.back().end(), LodInfo{ hop::INVALID_IDX, hop::INVALID_IDX, hop::INVALID_IDX, depth, false } );
   }

   return earliestEnd;
}

template <unsigned N>
void createAllLods( size_t i, hop::TimeStamp start, hop::TimeStamp end, hop::TimeDuration delta, std::array< LodInfo, LOD_COUNT >& lods, LodsArray& resultLods )
{
   lods[N] = createLod<N>( i, start, end, delta, lods[N], resultLods );
   createAllLods<N - 1>(i, start, end, delta, lods, resultLods);
}

template <>
void createAllLods<0>(size_t i, hop::TimeStamp start, hop::TimeStamp end, hop::TimeDuration delta, std::array< LodInfo, LOD_COUNT >& lods, LodsArray& resultLods)
{
   lods[0] = createLod<0>( i, start, end, delta, lods[0], resultLods );
}

void appendLods( LodsData& lodData, const Entries& entries )
{
   if( entries.ends.empty() ) return;

   HOP_ZONE( HOP_ZONE_COLOR_2 );
   HOP_PROF_FUNC();
   const std::array<TimeStamp, LOD_COUNT> earliestEnds = initializeLodsPerDepth( lodData, entries );

   const size_t startIndex = lodData.lastTraceIdx;
   const size_t entriesCount = entries.starts.size();
   for( size_t i = startIndex; i < entriesCount; ++i )
   {
      const TimeStamp start                   = entries.starts[i];
      const TimeStamp end                     = entries.ends[i];
      const Depth_t depth                     = entries.depths[i];
      std::array< LodInfo, LOD_COUNT >& lods  = lodData.latestLodPerDepth[depth];

      const TimeDuration delta = end - start;

      createAllLods<LOD_COUNT-1>( i, start, end, delta, lods, lodData.lods );
   }

   for( size_t i = 0; i < LOD_COUNT; ++i )
   {
      HOP_PROF_SPLIT( "Sorting lods" );
      const auto lodStartIt = lodData.lods[i].begin();
      auto it = std::lower_bound(lodStartIt, lodData.lods[i].end(), LodInfo{ earliestEnds[i], earliestEnds[i], 0, 0, false });
      auto dist = std::distance( lodStartIt, it );
      hop::insertionSort( lodStartIt + dist, lodData.lods[i].end() );
      assert_is_sorted( lodStartIt, lodData.lods[i].end() );
   }

   lodData.lastTraceIdx = entriesCount;
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