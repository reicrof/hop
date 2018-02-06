#ifndef UTILS_H_
#define UTILS_H_

#include <cassert>
#include <algorithm>

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
}

#endif  // UTILS_H_
