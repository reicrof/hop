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

constexpr TimeDuration LOD_CYCLES[] = {1000, 200000, 30000000, 300000000, 600000000, 6000000000, 70000000000, 200000000000, 600000000000 };
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
};

void setupLODResolution( uint32_t sreenResolutionX );

// Create and appends lods starting at start index.
void appendLods( LodsData& lodData, const Entries& entries, size_t startIndex );

 std::pair<size_t, size_t> visibleIndexSpan(
     const LodsArray& lodsArr,
     int lodLvl,
     TimeStamp absoluteStart,
     TimeStamp absoluteEnd );

}

#endif  // LOD_H_