#ifndef HOP_TEST_UTILS_H
#define HOP_TEST_UTILS_H

#include <cstdio>

#define HOP_TEST_ASSERT( x )                                      \
if( !(x) ) {                                                      \
   fprintf(stderr, "Test Failed at %s:%u\n", __FILE__, __LINE__); \
   return -1;                                                     \
}

#define HOP_TEST_ASSERT_RND( x, seed )                                                       \
if( !(x) ) {                                                                                 \
   fprintf(stderr, "Test with random seed %u Failed at %s:%u\n", seed, __FILE__, __LINE__);  \
   return -1;                                                                                \
}

#endif //HOP_TEST_UTILS_H