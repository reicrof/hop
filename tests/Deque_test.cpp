#include "common/Deque.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <vector>
#include <cmath>

std::vector< uint32_t > g_values;

template <typename T>
static bool testSingleIncrementDecrement(
    typename hop::Deque<T>::iterator it,
    uint32_t incCount )
{
   auto it2 = it;
   for( uint32_t i = 0; i < incCount; ++i )
   {
      ++it2;
   }
   for( uint32_t i = 0; i < incCount; ++i )
   {
      --it2;
   }

   return it2 == it;
}

template <typename T>
static bool testIncrementDecrement(
    typename hop::Deque<T>::iterator it,
    uint32_t incCount,
    uint32_t incValue )
{
   auto it2 = it;
   for( uint32_t i = 0; i < incCount; ++i )
   {
      it2 += incValue;
   }
   for( uint32_t i = 0; i < incCount; ++i )
   {
      it2 -= incValue;
   }

   return it2 == it;
}

template< typename T >
void testIterators( hop::Deque<T>& deq )
{
   auto it    = deq.begin();
   auto itEnd = deq.end();

   // Test value_type is defined
   using DequeType = hop::Deque<T>::value_type;

   // Test relational operators
   assert( it == it );
   assert( it != itEnd );
   assert( it < itEnd );
   assert( itEnd > it );
   assert( !( it < it ) );
   assert( !( it > it ) );
   assert( it >= it );
   assert( it <= it );

   // Test copy operation
   auto itCopy    = it;
   auto itCopyEnd = itEnd;
   assert( itCopy == it );
   assert( itCopyEnd == itEnd );
   assert( itCopy != itCopyEnd );

   // Test increment decrement operations
   assert( ( testSingleIncrementDecrement<T>( it, 1 ) ) );
   assert( ( testSingleIncrementDecrement<T>( it, 3 ) ) );
   assert( ( testSingleIncrementDecrement<T>( it, 2048 - 1 ) ) );
   assert( ( testSingleIncrementDecrement<T>( it, 2048 * 10 ) ) );
   assert( ( testIncrementDecrement<T>( it, 1, 1 ) ) );
   assert( ( testIncrementDecrement<T>( it, 3, 1 ) ) );
   assert( ( testIncrementDecrement<T>( it, 2048 - 1, 1 ) ) );
   assert( ( testIncrementDecrement<T>( it, 2048 * 10, 1 ) ) );

   uint32_t i = 0;
   for( ; i < deq.size() - 1; ++i )
   {
      ++it;
      assert( it != itEnd );
   }
   ++it;
   assert( it == itEnd );

   // Test lower_bound algorithm on d
   auto pastEnd = std::lower_bound( deq.begin(), deq.end(), 9999 );
   assert( pastEnd == itEnd );

   auto first = std::lower_bound( deq.begin(), deq.end(), 0 );
   assert( first == deq.begin() );

   auto it9 = std::lower_bound( deq.begin(), deq.end(), 9 );
   assert( it9 == deq.begin() + 9 );

   auto it42 = std::lower_bound( deq.begin(), deq.end(), 42 );
   assert( it42 == deq.begin() + 42 );
}

void testAppend()
{
   hop::Deque< uint32_t > deq, appendDeq;
   deq.append( g_values.data(), g_values.size() );
 
   uint32_t valueOffset = 10;
   appendDeq.append( deq.begin() + valueOffset, deq.begin() + valueOffset + 50 );
   for( uint32_t i = 0; i < appendDeq.size(); ++i )
   {
      auto value = appendDeq[i];
      assert( value == i + valueOffset );
   }

   appendDeq.clear();

   // Append data that spans 2 blocks
   valueOffset = 500;
   appendDeq.append( deq.begin() + valueOffset , deq.begin() + valueOffset + 200 );
   for( uint32_t i = 0; i < appendDeq.size(); ++i )
   {
      auto value = appendDeq[i];
      assert( value == i + valueOffset );
   }
}

int main()
{
   hop::block_allocator::initialize( 2048, 32 );
   hop::Deque< uint32_t > deq;

   auto it    = deq.begin();
   auto itEnd = deq.end();
   assert( it == itEnd );
   assert( deq.size() == 0 );

   g_values.resize(1024);
   std::iota( g_values.begin(), g_values.end(), 0 );
   deq.append( g_values.data(), g_values.size() );

    assert( std::is_sorted( deq.begin(), deq.end() ) );

   //std::cout << deq;

   testIterators( deq );
   testAppend();

   // Testing accessors
   for( size_t i = 0; i < deq.size(); ++i )
   {
      auto value = deq[i];
      assert( value == i );
   }

   deq.clear();
   assert( deq.size() == 0 );

   hop::block_allocator::terminate();
}