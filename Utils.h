#ifndef UTILS_H_
#define UTILS_H_

#include <cassert>

namespace vdbg
{
template <typename I>
inline void assert_is_sorted( I first, I last )
{
	(void)first;(void)last;
#ifdef VDBG_ASSERT_IS_SORTED
   assert( std::is_sorted( first, last ) );
#endif
}
}

#endif  // UTILS_H_
