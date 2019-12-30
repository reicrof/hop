#ifndef LOD_H_
#define LOD_H_

#include "Hop.h"

#include <array>
#include <deque>
#include <vector>

namespace hop
{

struct Entries;
//struct LockWaitData;

extern TimeDuration LOD_CYCLES[9];
constexpr int LOD_COUNT = sizeof( LOD_CYCLES ) / sizeof( LOD_CYCLES[0] );

struct LodInfo
{
   TimeStamp start, end;
   size_t index;
   Depth_t depth;
   bool loded;
   bool operator<( const LodInfo& rhs ) const noexcept { return end < rhs.end; }
};

using LodsArray = std::array< std::deque< LodInfo >, LOD_COUNT >;
struct LodsData
{
   LodsArray lods;
   std::vector< std::array< LodInfo, LOD_COUNT > > latestLodPerDepth;
   size_t lastTraceIdx{0};
};

void setupLODResolution( uint32_t sreenResolutionX );

// Create and appends lods
void appendLods( LodsData& lodData, const Entries& entries );

 std::pair<size_t, size_t> visibleIndexSpan(
     const LodsArray& lodsArr,
     int lodLvl,
     TimeStamp absoluteStart,
     TimeStamp absoluteEnd,
     Depth_t minDepth );

}

#endif  // LOD_H_