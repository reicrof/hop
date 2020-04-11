#include "TestUtils.h"
#include <common/HashSet.h>

#include <cassert>
#include <random>
#include <unordered_set>

int main()
{
   std::random_device r;
   std::default_random_engine e1( r() );
   std::uniform_int_distribution<uint64_t> uniform_dist( 1, 0xFFFFFFFF );

   hop_hash_set_t hs = hop_hash_set_create();
   std::unordered_set< void* > reference;

   for( size_t i = 0; i < 1000000; ++i )
   {
      uint64_t randomValue = uniform_dist( e1 );
      bool inserted = hop_hash_set_insert( hs, (void*)randomValue );
      auto referenceInsert = reference.insert( (void*)randomValue );
      HOP_TEST_ASSERT( inserted == referenceInsert.second );

      bool insertFailure = hop_hash_set_insert(hs, (void*)randomValue);
      HOP_TEST_ASSERT( !insertFailure );

      HOP_TEST_ASSERT( reference.size() == hop_hash_set_count( hs ) );
   }

   hop_hash_set_clear( hs );
   hop_hash_set_destroy( hs );
}