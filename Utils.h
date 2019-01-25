#ifndef UTILS_H_
#define UTILS_H_

#include "Hop.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// #define HOP_ASSERT_IS_SORTED

namespace hop
{

uint64_t getCpuFreqHz();

int formatCyclesDurationToDisplay( uint64_t duration, char* str, size_t strSize, bool asCycles );

int formatCyclesTimepointToDisplay(
    int64_t timepoint,
    uint64_t totalCyclesInScreen,
    char* str,
    size_t strSize,
    bool asCycles );

void formatSizeInBytesToDisplay( size_t sizeInBytes, char* str, size_t strSize );

template <typename I>
inline void assert_is_sorted( I first, I last )
{
   (void)first;(void)last;
#ifdef HOP_ASSERT_IS_SORTED
   assert( std::is_sorted( first, last ) );
#endif
}

template <typename T>
inline T clamp(const T& val, const T& lower, const T& upper)
{
   return std::max( lower, std::min( val, upper ) );
}

template <typename T>
inline int sign( const T& val )
{
    return (T(0) < val) - (val < T(0));
}

// TODO template these 2 functions so they can be used with different time ratios
template< typename T = uint64_t >
static inline T cyclesToPxl( double windowWidth, uint64_t timelineRange, uint64_t cycles )
{
   const double cyclePerPxl = timelineRange / windowWidth;
   return static_cast<T>( (double)cycles / cyclePerPxl );
}

template< typename T = uint64_t >
static inline T pxlToCycles( double windowWidth, uint64_t timelineRange, double pxl )
{
   const double cyclesPerPxl = timelineRange / windowWidth;
   return static_cast<T>( cyclesPerPxl * pxl );
}

static inline int64_t cyclesToNanos( int64_t cycles )
{
   static const double cpuFreqGhz = getCpuFreqHz() / 1000000000.0;
   return (cycles / cpuFreqGhz) + 0.5;
}

static inline uint64_t nanosToCycles( uint64_t nanos )
{
   static const double cpuFreqGhz = getCpuFreqHz() / 1000000000.0;
   return (nanos * cpuFreqGhz) + 0.5;
}

inline uint32_t addColorWithClamping( uint32_t c1, uint32_t c2 )
{
   unsigned char r = clamp( ((c1 >> 0) & 0xFF) + ((c2 >> 0) & 0xFF), 0u, 255u );
   unsigned char g = clamp( ((c1 >> 8) & 0xFF) + ((c2 >> 8) & 0xFF), 0u, 255u );
   unsigned char b = clamp( ((c1 >> 16) & 0xFF) + ((c2 >> 16) & 0xFF), 0u, 255u );
   unsigned char a = clamp( ((c1 >> 24) & 0xFF) + ((c2 >> 24) & 0xFF), 0u, 255u );

   return ( r | (g<<8) | (b<<16) | (a<<24) );
}

inline uint32_t subColorWithClamping( uint32_t c1, uint32_t c2 )
{
   unsigned char r = clamp( ((c1 >> 0) & 0xFF) - ((c2 >> 0) & 0xFF), 0u, 255u );
   unsigned char g = clamp( ((c1 >> 8) & 0xFF) - ((c2 >> 8) & 0xFF), 0u, 255u );
   unsigned char b = clamp( ((c1 >> 16) & 0xFF) - ((c2 >> 16) & 0xFF), 0u, 255u );
   unsigned char a = clamp( ((c1 >> 24) & 0xFF) - ((c2 >> 24) & 0xFF), 0u, 255u );

   return ( r | (g<<8) | (b<<16) | (a<<24) );
}

inline bool ptInRect( float ptx, float pty, float ax, float ay, float bx, float by )
{
   if( ptx < ax || ptx > bx ) return false;
   if( pty < ay || pty > by ) return false;

   return true;
}

inline bool ptInCircle( float ptx, float pty, float centerx, float centery, float radius )
{
   const float dx = ptx - centerx;
   const float dy = pty - centery;
   return dx * dx + dy * dy <= radius * radius;
}



} // namespace hop

#endif  // UTILS_H_
