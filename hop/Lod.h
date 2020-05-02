#ifndef LOD_H_
#define LOD_H_

#include "Hop.h"
#include "common/Deque.h"

#include <array>
#include <vector>

namespace hop
{

struct Entries;
//struct LockWaitData;

extern hop_timeduration_t LOD_CYCLES[9];
constexpr int LOD_COUNT = sizeof( LOD_CYCLES ) / sizeof( LOD_CYCLES[0] );

struct LodInfo
{
   hop_timestamp_t start, end;
   size_t index;
   hop_depth_t depth;
   bool loded;
   bool operator<( const LodInfo& rhs ) const noexcept { return end < rhs.end; }
};

using LodsArray = std::array< hop::Deque< LodInfo >, LOD_COUNT >;
struct LodsData
{
   LodsArray lods;
   size_t idOffset{0};
};

void setupLODResolution( uint32_t sreenResolutionX );

// Create and appends lods
void appendLods( LodsData& lodData, const Entries& entries );

void appendCoreEventLods(
    LodsData& lodData,
    const Entries& entries,
    const hop::Deque<hop_core_t>& cores );

std::pair<size_t, size_t> visibleIndexSpan(
    const LodsArray& lodsArr,
    int lodLvl,
    hop_timestamp_t absoluteStart,
    hop_timestamp_t absoluteEnd,
    hop_depth_t minDepth );

}

#endif  // LOD_H_