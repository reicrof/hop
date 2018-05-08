#ifndef UTILS_H_
#define UTILS_H_

#include <cstdio>
#include <cassert>
#include <algorithm>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//#define HOP_ASSERT_IS_SORTED

namespace hop
{
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

// TODO template these 2 functions so they can be used with different time ratios
template< typename T = uint64_t >
static inline T nanosToPxl( double windowWidth, uint64_t timelineRange, uint64_t ns )
{
   const double nsPerPxl = timelineRange / windowWidth;
   return static_cast<T>( (double)ns / nsPerPxl );
}

template< typename T = uint64_t >
static inline T pxlToNanos( double windowWidth, uint64_t timelineRange, double pxl )
{
   const double nsPerPxl = timelineRange / windowWidth;
   return static_cast<T>( nsPerPxl * pxl );
}

inline int formatNanosDurationToDisplay( uint64_t duration, char* str, size_t strSize )
{
   if ( duration < 1000 )
   {
      return snprintf( str, strSize, "%" PRIu64 " ns", duration );
   }
   else if ( duration < 1000000 )
   {
      return snprintf( str, strSize, "%.3f us", duration * 0.001f );
   }
   else if ( duration < 1000000000 )
   {
      return snprintf( str, strSize, "%.3f ms", duration * 0.000001f );
   }
   else
   {
      return snprintf( str, strSize, "%.3f s", duration * 0.000000001f );
   }
}

inline void formatNanosTimepointToDisplay(int64_t timepoint, uint64_t totalNanosInScreen, char* str, size_t strSize)
{
   if (totalNanosInScreen < 1000)
   {
      snprintf(str, strSize, "%" PRId64 " ns", timepoint);
   }
   else if (totalNanosInScreen < 1000000)
   {
      snprintf(str, strSize, "%.3f us", timepoint * 0.001f);
   }
   else if (totalNanosInScreen < 1000000000)
   {
      snprintf(str, strSize, "%.3f ms", timepoint * 0.000001f);
   }
   else
   {
      snprintf(str, strSize, "%.3f s", timepoint * 0.000000001f);
   }
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

inline void formatSizeInBytesToDisplay( size_t sizeInBytes, char* str, size_t strSize )
{
   if (sizeInBytes < 1000)
   {
      snprintf(str, strSize, "%d B", (int)sizeInBytes);
   }
   else if (sizeInBytes < 1000000)
   {
      snprintf(str, strSize, "%.3f kB", sizeInBytes / 1000.0f);
   }
   else if (sizeInBytes < 1000000000)
   {
      snprintf(str, strSize, "%.3f MB",  sizeInBytes / 1000000.0f);
   }
   else
   {
      snprintf(str, strSize, "%.3f GB",  sizeInBytes / 1000000000.0f);
   }
}

} // namespace hop

#endif  // UTILS_H_
