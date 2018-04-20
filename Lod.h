#ifndef LOD_H_
#define LOD_H_

#include "Hop.h"

#include <array>
#include <vector>

namespace hop
{

struct DisplayableTraces;

constexpr TimeDuration LOD_NANOS[] = {3000000, 30000000, 300000000, 600000000, 1000000000, 3000000000, 6000000000, 30000000000, 70000000000, 600000000000, 50000000000000};
constexpr int LOD_COUNT = sizeof( LOD_NANOS ) / sizeof( LOD_NANOS[0] );

struct LodInfo
{
   TimeStamp end;
   TimeDuration delta;
   size_t traceIndex;
   TDepth_t depth;
   bool isLoded;
   bool operator<( const LodInfo& rhs ) const noexcept { return end < rhs.end; }
};

using LodsArray = std::array< std::vector< LodInfo >, LOD_COUNT >;

void setupLODResolution( uint32_t sreenResolutionX );

// Returns a vector of LodInfo for each LOD level. The lod infos are sorted.
LodsArray computeLods( const DisplayableTraces& traces, size_t idOffset );
// Appends lods infos
void appendLods( LodsArray& dst, const LodsArray& src );

}

#endif  // LOD_H_