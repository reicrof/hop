#include "common/Deque.h"
#include "common/BlockAllocator.h"
#include "tests/TestUtils.h"

#include <algorithm>
#include <numeric>
#include <vector>
#include <cmath>

std::vector< uint32_t > g_values;

template <typename T>
static bool testSingleIncrementDecrement(
    T it,
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
    T it,
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

   // Test that value_type is defined
   (void)sizeof( typename hop::Deque<T>::value_type );

   // Test relational operators
   HOP_TEST_ASSERT( it == it );
   HOP_TEST_ASSERT( it != itEnd );
   HOP_TEST_ASSERT( it < itEnd );
   HOP_TEST_ASSERT( itEnd > it );
   HOP_TEST_ASSERT( !( it < it ) );
   HOP_TEST_ASSERT( !( it > it ) );
   HOP_TEST_ASSERT( it >= it );
   HOP_TEST_ASSERT( it <= it );

   // Test copy operation
   auto itCopy    = it;
   auto itCopyEnd = itEnd;
   HOP_TEST_ASSERT( itCopy == it );
   HOP_TEST_ASSERT( itCopyEnd == itEnd );
   HOP_TEST_ASSERT( itCopy != itCopyEnd );

   // Test increment decrement operations
   HOP_TEST_ASSERT( ( testSingleIncrementDecrement( it, 1 ) ) );
   HOP_TEST_ASSERT( ( testSingleIncrementDecrement( it, 3 ) ) );
   HOP_TEST_ASSERT( ( testSingleIncrementDecrement( it, HOP_BLK_SIZE_BYTES - 1 ) ) );
   HOP_TEST_ASSERT( ( testSingleIncrementDecrement( it, HOP_BLK_SIZE_BYTES * 10 ) ) );
   HOP_TEST_ASSERT( ( testIncrementDecrement( it, 1, 1 ) ) );
   HOP_TEST_ASSERT( ( testIncrementDecrement( it, 3, 1 ) ) );
   HOP_TEST_ASSERT( ( testIncrementDecrement( it, HOP_BLK_SIZE_BYTES - 1, 1 ) ) );
   HOP_TEST_ASSERT( ( testIncrementDecrement( it, HOP_BLK_SIZE_BYTES * 10, 1 ) ) );

   uint32_t i = 0;
   for( ; i < deq.size() - 1; ++i )
   {
      ++it;
      HOP_TEST_ASSERT( it != itEnd );
   }
   ++it;
   HOP_TEST_ASSERT( it == itEnd );

   // Test lower_bound algorithm on d
   auto pastEnd = std::lower_bound( deq.begin(), deq.end(), deq.size() + 1 );
   HOP_TEST_ASSERT( pastEnd == itEnd );

   auto first = std::lower_bound( deq.begin(), deq.end(), 0 );
   HOP_TEST_ASSERT( first == deq.begin() );

   auto it9 = std::lower_bound( deq.begin(), deq.end(), 9 );
   HOP_TEST_ASSERT( it9 == deq.begin() + 9 );

   auto it42 = std::lower_bound( deq.begin(), deq.end(), 42 );
   HOP_TEST_ASSERT( it42 == deq.begin() + 42 );
}

void testCopy()
{
   hop::Deque< uint32_t > deq;
   deq.append( g_values.data(), g_values.size() );

   hop::Deque< uint32_t > deq2( deq );
   HOP_TEST_ASSERT( deq.size() == deq2.size() );

   deq.erase( deq.begin() + 20, deq.begin() + 100 );
   deq2 = deq;
   HOP_TEST_ASSERT( deq.size() == deq2.size() );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );

   deq.erase(deq.begin(), deq.begin() + hop::Deque<uint32_t>::COUNT_PER_BLOCK + 50);
   deq2 = deq;
   HOP_TEST_ASSERT(deq.size() == deq2.size());
   HOP_TEST_ASSERT(std::is_sorted(deq.begin(), deq.end()));
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
      HOP_TEST_ASSERT( value == i + valueOffset );
   }

   // Append a second sub block and make sure we handle the "partial" src block correctly
   auto newSubBlock = deq.begin() + valueOffset + 50;
   appendDeq.append( newSubBlock, newSubBlock + 20 );
   for( uint32_t i = 0; i < appendDeq.size(); ++i )
   {
      auto value = appendDeq[i];
      HOP_TEST_ASSERT( value == i + valueOffset );
   }

   // Clear for upcoming tests
   appendDeq.clear();

   // Append data that spans 2 blocks from source
   valueOffset = 500;
   const uint32_t valueCount  = 250;
   appendDeq.append( deq.begin() + valueOffset , deq.begin() + valueOffset + valueCount );
   for( uint32_t i = 0; i < valueCount; ++i )
   {
      auto value = appendDeq[i];
      HOP_TEST_ASSERT( value == i + valueOffset );
   }

   appendDeq.clear();

   // Append data that spans 2 blocks in destination
   appendDeq.append( deq.begin(), deq.end() - 50 );
   for( uint32_t i = 0; i < appendDeq.size(); ++i )
   {
      auto value = appendDeq[i];
      HOP_TEST_ASSERT( value == i );
   }

   { // Append a full block than a smaller one
      appendDeq.clear();
      appendDeq.append( deq.begin(),deq.begin() + hop::Deque<uint32_t>::COUNT_PER_BLOCK );
      appendDeq.append( deq.begin(),deq.begin() + 25 );
   }
}

void testErase()
{
   hop::Deque<uint32_t> deq;
   deq.append( g_values.data(), g_values.size() );

   uint32_t removedCount = 0;

   // Erase nothing
   deq.erase( deq.begin(), deq.begin() );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
   HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );

   // Erase nothing
   deq.erase( deq.end(), deq.end() );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
   HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );

   // Erase nothing
   deq.erase( deq.begin() + 10, deq.begin() + 10 );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
   HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );

   // Erase the last element. Simple case where no moving is performed
   removedCount += 1;
   deq.erase( deq.end() - 1 );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
   HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );

   // Erase the first element. All the block will need to be shifted by 1
   removedCount += 1;
   deq.erase( deq.begin() );
   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
   HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );

   /*  Erase range element than spans 2 blocks with the right one
    * having enough remaining elements to fill the left one
    * [XXXX------][--XXXXXXXX]
    * ->
    * [XXXXXXXXXX][--------XX]
    * ->
    * [XXXXXXXXXX][XX--------]
    */
   {
      removedCount += 50;
      auto erIt = deq.begin() + hop::Deque<uint32_t>::COUNT_PER_BLOCK - 12;
      deq.erase( erIt, erIt + 50 );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );
   }

   /*
    * Test removing a range that spans 2 blocks, with the right one
    * not having enough element to fill the left and no more block
    * after that
    * [XX--------][--------XX]
    * ->
    * [XXXX------][----------]
    * ->
    * [XXXX------]
    */
   {
      deq.clear();
      deq.append( g_values.data(), g_values.size() );
      removedCount = hop::Deque<uint32_t>::COUNT_PER_BLOCK + hop::Deque<uint32_t>::COUNT_PER_BLOCK / 2;
      auto errFrom = deq.begin() + 10;
      auto errTo   = errFrom + removedCount;
      deq.erase( errFrom, errTo  );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );
   }

      /*
    * Test removing a range ends directly on the end of the block
    * block having enough
    * [XXX-------][XXXXXXXXXX]
    * ->
    * [XXXXXXXXXX][-------XXX]
    */
   {
      deq.clear();
      deq.append( g_values.data(), g_values.size() );
      removedCount      = hop::Deque<uint32_t>::COUNT_PER_BLOCK / 2;
      auto errFrom      = deq.begin() + removedCount/2;
      auto errTo        = errFrom + removedCount;
      deq.erase( errFrom, errTo );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );
   }

   /*
    * Test removing a range that spans 2 blocks, with the right one
    * fitting exactly the gap created in the left one
    * block having enough
    * [XXX-------][---XXXXXXX]
    * ->
    * [XXXXXXXXXX][----------]
    * ->
    * [XXXXXXXXXX]
    */
   {
      deq.clear();
      deq.append( g_values.data(), g_values.size() );
      removedCount      = hop::Deque<uint32_t>::COUNT_PER_BLOCK;
      auto errFrom      = deq.begin() + removedCount/2;
      auto errTo        = errFrom + removedCount;
      deq.erase( errFrom, errTo );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == g_values.size() - removedCount );
   }

   /*
    * Test removing a range that spans 2 blocks, with the right one
    * not having enough element to fill the left, but with a next
    * block having enough
    * [XX--------][--------XX][XXXXXXXXXX]
    * ->
    * [XXXX------][----------][XXXXXXXXXX]
    * ->
    * [XXXX------][XXXXXXXXXX]
    * ->
    * [XXXXXXXXXX][------XXXX]
    * ->
    * [XXXXXXXXXX][XXXX------]
    */
   {
      deq.clear();
      std::vector<uint32_t> veryLargeValues( g_values.size() * 2 );
      std::iota( veryLargeValues.begin(), veryLargeValues.end(), 0 );
      deq.append( veryLargeValues.data(), veryLargeValues.size() );
      removedCount = hop::Deque<uint32_t>::COUNT_PER_BLOCK + hop::Deque<uint32_t>::COUNT_PER_BLOCK / 2;
      auto errFrom = deq.begin() + 10;
      auto errTo   = errFrom + removedCount;
      deq.erase( errFrom, errTo  );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == veryLargeValues.size() - removedCount );
   }

   /*
    * Remove multiple blocks
    */
    {
      deq.clear();
      std::vector<uint32_t> veryLargeValues( g_values.size() * 2 );
      std::iota( veryLargeValues.begin(), veryLargeValues.end(), 0 );
      deq.append( veryLargeValues.data(), veryLargeValues.size() );
      removedCount = g_values.size();
      auto errFrom = deq.begin() + 10;
      auto errTo   = errFrom + removedCount;
      deq.erase( errFrom, errTo  );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );
      HOP_TEST_ASSERT( deq.size() == veryLargeValues.size() - removedCount );
   }

   /*
    * Remove everything, by using iterators
    */
    {
      deq.clear();
      deq.append( g_values.data(), g_values.size() );
      deq.erase( deq.begin(), deq.end() );
      HOP_TEST_ASSERT( deq.size() == 0 );
   }

    /*
    * Push one block + 1 element and remove the last element
    */
    {
      deq.clear();
      deq.append( g_values.data(), hop::Deque<uint32_t>::COUNT_PER_BLOCK );
      deq.append( g_values.data(), 1 );
      deq.erase( deq.end() -1, deq.end() );
      HOP_TEST_ASSERT( deq.size() == hop::Deque<uint32_t>::COUNT_PER_BLOCK );
   }
   
   /*
    * Create a partial right block by removing some in the first block,
    * then remove a span larger than 1 block so we test the span accross
    * an incomplete block
    */
   {
      deq.clear();
      deq.append( g_values.data(), g_values.size() );
      deq.erase( deq.begin() + 20, deq.begin() + 100 );
      HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );

      deq.erase(deq.begin(), deq.begin() + hop::Deque<uint32_t>::COUNT_PER_BLOCK + 50);
      HOP_TEST_ASSERT(std::is_sorted(deq.begin(), deq.end()));  
   }
}

int main()
{
   hop::block_allocator::initialize( 2048 * HOP_BLK_SIZE_BYTES );

   {
   hop::Deque< uint32_t > deq;

   auto it    = deq.begin();
   auto itEnd = deq.end();
   HOP_TEST_ASSERT( it == itEnd );
   HOP_TEST_ASSERT( deq.size() == 0 );

   const size_t valueCount = hop::Deque<uint32_t>::COUNT_PER_BLOCK * 4;
   g_values.resize( valueCount );
   std::iota( g_values.begin(), g_values.end(), 0 );
   deq.append( g_values.data(), g_values.size() );

   HOP_TEST_ASSERT( deq.back() == valueCount -1 );

   HOP_TEST_ASSERT( std::is_sorted( deq.begin(), deq.end() ) );

   testIterators( deq );
   testAppend();
   testErase();
   testCopy();

   // Testing accessors
   for( size_t i = 0; i < deq.size(); ++i )
   {
      auto value = deq[i];
      HOP_TEST_ASSERT( value == i );
   }

   deq.clear();
   HOP_TEST_ASSERT( deq.size() == 0 );
   }

   hop::block_allocator::terminate();
}