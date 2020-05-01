#ifndef UTILS_H_
#define UTILS_H_

#include "Hop.h"

#include <algorithm>
#include <cassert>
#include <cctype> // toupper
#include <chrono>
#include <cstdio>
#include <functional>
#include <math.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//#define HOP_ASSERT_IS_SORTED

namespace hop
{
bool supportsRDTSCP();

bool supportsConstantTSC();

float getCpuFreqGHz();

int formatCyclesDurationToDisplay(
    uint64_t duration,
    char* str,
    size_t strSize,
    bool asCycles,
    float cpuFreqGHz );

int formatCyclesTimepointToDisplay(
    int64_t timepoint,
    uint64_t totalCyclesInScreen,
    char* str,
    size_t strSize,
    bool asCycles,
    float cpuFreqGHz );

void formatSizeInBytesToDisplay( size_t sizeInBytes, char* str, size_t strSize );

template <typename I, class Compare = std::less<typename I::value_type> >
inline void assert_is_sorted( I first, I last, Compare comp = Compare() )
{
   (void)first;(void)last;(void)comp;
#ifdef HOP_ASSERT_IS_SORTED
   assert( std::is_sorted( first, last, comp ) );
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
template< typename T = float >
inline T cyclesToPxl( double windowWidth, uint64_t timelineRange, int64_t cycles )
{
   const double cyclePerPxl = timelineRange / windowWidth;
   return static_cast<T>( (double)cycles / cyclePerPxl );
}

template< typename T = uint64_t >
inline T pxlToCycles( double windowWidth, uint64_t timelineRange, double pxl )
{
   const double cyclesPerPxl = timelineRange / windowWidth;
   return static_cast<T>( cyclesPerPxl * pxl );
}

inline int64_t cyclesToNanos( int64_t cycles, float cpuFreqGHz )
{
   return llround( cycles / cpuFreqGHz);
}

inline uint64_t nanosToCycles( uint64_t nanos, float cpuFreqGHz )
{
   return llround(nanos * cpuFreqGHz);
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

inline uint32_t nextPow2( uint32_t x )
{
   x--;
   x |= x >> 1;
   x |= x >> 2;
   x |= x >> 4;
   x |= x >> 8;
   x |= x >> 16;
   x++;
   return x;
}

int findSubstrNoCase(
    const char* haystack,
    uint32_t haystackSize,
    const char* needle,
    uint32_t needleSize );

int getPIDFromString( const char* str );

template< typename IT >
void insertionSort( IT begin, IT end )
{
   if( begin == end ) return;

   std::iter_swap( begin, std::min_element( begin, end ) );
   for( IT b = begin; ++b < end; begin = b )
   {
      for(IT c = b; *c < *begin; --c, --begin)
      {
         std::iter_swap(begin, c);
      }
   }
}

} // namespace hop

#endif  // UTILS_H_
