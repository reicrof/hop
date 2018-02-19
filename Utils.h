#ifndef UTILS_H_
#define UTILS_H_

#include <cassert>
#include <algorithm>

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
static inline T microsToPxl( double windowWidth, int64_t usToDisplay, int64_t us )
{
   const double usPerPxl = usToDisplay / windowWidth;
   return static_cast<T>( (double)us / usPerPxl );
}

template< typename T = uint64_t >
static inline T pxlToMicros( double windowWidth, int64_t usToDisplay, int64_t pxl )
{
   const double usPerPxl = usToDisplay / windowWidth;
   return static_cast<T>( usPerPxl * (double)pxl );
}

inline void formatMicrosDurationToDisplay( int64_t usToDisplay, char* str, size_t strSize )
{
   if ( usToDisplay < 1000 )
   {
      snprintf( str, strSize, "%d us", (int)usToDisplay );
   }
   else if ( usToDisplay < 1000000 )
   {
     snprintf( str, strSize, "%.3f ms", usToDisplay * 0.001f );
   }
   else if ( usToDisplay < 1000000000 )
   {
       snprintf( str, strSize, "%.3f s", usToDisplay * 0.000001f );
   }
   else
   {
       snprintf( str, strSize, "%.3f s", usToDisplay * 2.77778e-10 );
   }
}

}

#endif  // UTILS_H_
