#include "TestUtils.h"
#include <tests/HashSet.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

int main()
{
   std::random_device r;
   std::default_random_engine e1( r() );
   std::uniform_int_distribution<uint64_t> uniform_dist( 1, 0xFFFFFFFF );

   hop_hash_set_t hs = hop_hash_set_create();
   std::unordered_set< void* > reference;

   const size_t itCount = 1000000;
   std::vector< uint64_t > randNums( itCount );
   for( size_t i = 0; i < itCount; ++i )
   {
      randNums[ i ] = uniform_dist( e1 );
   }

   for( size_t i = 0; i < itCount; ++i )
   {
      bool inserted = hop_hash_set_insert( hs, (void*)randNums[i] );
      auto referenceInsert = reference.insert( (void*)randNums[i] );
      HOP_TEST_ASSERT( inserted == referenceInsert.second );

      bool insertFailure = hop_hash_set_insert(hs, (void*)randNums[i]);
      HOP_TEST_ASSERT( !insertFailure );

      HOP_TEST_ASSERT( (int)reference.size() == hop_hash_set_count( hs ) );
   }

   // Clear both to bench
   hop_hash_set_clear( hs );
   reference.clear();

   // Do some benchmarks

   printf( "Benchmarking insertion on an empty (but pre-allocated) hash set\n" );
   {
      auto hopStart = std::chrono::high_resolution_clock::now();
      for( size_t i = 0; i < itCount; ++i )
      {
         hop_hash_set_insert( hs, (void*)randNums[i] );
      }
      auto hopEnd = std::chrono::high_resolution_clock::now();

      auto refStart = std::chrono::high_resolution_clock::now();
      for( size_t i = 0; i < itCount; ++i )
      {
         reference.insert( (void*)randNums[i] );
      }
      auto refEnd = std::chrono::high_resolution_clock::now();

      auto hopDuration =
          std::chrono::duration_cast<std::chrono::microseconds>( hopEnd - hopStart ).count();
      auto refDuration =
          std::chrono::duration_cast<std::chrono::microseconds>( refEnd - refStart ).count();
      std::cout << "Hop hash set took : " << hopDuration
                << " us\nstd hash set took : " << refDuration << " us\n";
   }

   printf( "Benchmarking lookups of exsiting value\n" );
   {
      auto hopStart = std::chrono::high_resolution_clock::now();
      for( size_t i = 0; i < itCount; ++i )
      {
         hop_hash_set_insert( hs, (void*)randNums[i] );
      }
      auto hopEnd = std::chrono::high_resolution_clock::now();

      auto refStart = std::chrono::high_resolution_clock::now();
      for( size_t i = 0; i < itCount; ++i )
      {
         reference.insert( (void*)randNums[i] );
      }
      auto refEnd = std::chrono::high_resolution_clock::now();

      auto hopDuration =
          std::chrono::duration_cast<std::chrono::microseconds>( hopEnd - hopStart ).count();
      auto refDuration =
          std::chrono::duration_cast<std::chrono::microseconds>( refEnd - refStart ).count();
      std::cout << "Hop hash set took : " << hopDuration
                << " us\nstd hash set took : " << refDuration << " us\n";
   }

   hop_hash_set_destroy( hs );
}