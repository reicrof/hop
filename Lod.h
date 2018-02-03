#ifndef LOD_H_
#define LOD_H_

#include "vdbg.h"

#include <array>
#include <vector>

namespace vdbg
{

struct DisplayableTraces;

constexpr size_t LOD_MICROS[] = {30000, 300000, 600000, 6000000, 30000000, 600000000, 50000000000};
constexpr long LOD_MIN_SIZE_MICROS[] = {80, 1200, 2500, 5000, 10000, 700000, 1000000};
constexpr int LOD_COUNT = sizeof( LOD_MICROS ) / sizeof( LOD_MICROS[0] );

struct LodInfo
{
   TimeStamp end, delta;
   size_t traceIndex;
   TDepth_t depth;
   bool isLoded;
   bool operator<( const LodInfo& rhs ) const noexcept { return end < rhs.end; }
};

std::array< std::vector< LodInfo >, LOD_COUNT > computeLods( const DisplayableTraces& traces );

}

#endif  // LOD_H_