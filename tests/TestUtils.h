#ifndef HOP_TEST_UTILS_H
#define HOP_TEST_UTILS_H

// Since this is for tests, we want to keep the assert in
#undef NDEBUG
#include <assert.h>

#include <cstdio>

#define HOP_TEST_ASSERT( x )                                      \
if( !(x) ) {                                                      \
   assert( (x) );                                                 \
   fprintf(stderr, "Test Failed at %s:%u\n", __FILE__, __LINE__); \
}

#define HOP_TEST_ASSERT_RND( x, seed )                                                       \
if( !(x) ) {                                                                                 \
   assert( (x) );                                                                            \
   fprintf(stderr, "Test with random seed %u Failed at %s:%u\n", seed, __FILE__, __LINE__);  \
}

#endif //HOP_TEST_UTILS_H