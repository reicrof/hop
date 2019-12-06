#include "BlockStreamView.h"
#include "platform/Platform.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <vector>
#include <cmath>

const char* g_testFilePath;
std::vector<unsigned> g_data;

template <typename T>
static void writeDataToTempFile( const T* data, uint32_t count )
{
   FILE* writeFile = fopen( g_testFilePath, "w+b" );
   fwrite( data, sizeof( T ), count, writeFile );
   fclose( writeFile );
}

template <typename T, unsigned BLK_SIZE>
static bool testSingleIncrementDecrement(
    typename hop::BlockStreamView<T, BLK_SIZE>::iterator it,
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

template <typename T, unsigned BLK_SIZE>
static bool testIncrementDecrement(
    typename hop::BlockStreamView<T, BLK_SIZE>::iterator it,
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

template <unsigned BLK_SIZE>
static void testIterators()
{
   writeDataToTempFile( g_data.data(), 500 );
   hop::BlockStreamView<unsigned, BLK_SIZE> bsview( 16, g_testFilePath );
   auto it    = bsview.begin();
   auto itEnd = bsview.end();
   assert( it == itEnd );
   assert( bsview.size() == 0 );

   // Read data and update iterators
   const uint32_t blocksToRead = std::ceil(100.0f / BLK_SIZE);
   const uint32_t manyBlocksCount = bsview.readBlock( 0, blocksToRead );
   assert( manyBlocksCount == blocksToRead * BLK_SIZE );
   assert( bsview.size() == blocksToRead * BLK_SIZE );

   std::cout << bsview;

   // Update iterators after reading
   it    = bsview.begin();
   itEnd = bsview.end();

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
   assert( ( testSingleIncrementDecrement<unsigned, BLK_SIZE>( it, 1 ) ) );
   assert( ( testSingleIncrementDecrement<unsigned, BLK_SIZE>( it, 3 ) ) );
   assert( ( testSingleIncrementDecrement<unsigned, BLK_SIZE>( it, BLK_SIZE - 1 ) ) );
   assert( ( testSingleIncrementDecrement<unsigned, BLK_SIZE>( it, BLK_SIZE * 10 ) ) );

   assert( ( testIncrementDecrement<unsigned, BLK_SIZE>( it, 1, 1 ) ) );
   assert( ( testIncrementDecrement<unsigned, BLK_SIZE>( it, 3, 1 ) ) );
   assert( ( testIncrementDecrement<unsigned, BLK_SIZE>( it, BLK_SIZE - 1, 1 ) ) );
   assert( ( testIncrementDecrement<unsigned, BLK_SIZE>( it, BLK_SIZE * 10, 1 ) ) );

   for( uint32_t i = 0; i < bsview.size() - 1; ++i )
   {
      ++it;
      assert( it != itEnd );
   }
   ++it;
   assert( it == itEnd );

   // Test lower_bound algorithm on BSView
   auto pastEnd = std::lower_bound( bsview.begin(), bsview.end(), 9999 );
   assert( pastEnd == itEnd );

   auto first = std::lower_bound( bsview.begin(), bsview.end(), 0 );
   assert( first == bsview.begin() );

   auto it9 = std::lower_bound( bsview.begin(), bsview.end(), 9 );
   assert( it9 == bsview.begin() + 9 );

   auto it42 = std::lower_bound( bsview.begin(), bsview.end(), 42 );
   assert( it42 == bsview.begin() + 42 );
}

int main()
{
   // Init temp file path first
   char tempPath[512];
   hop::getTempFolderPath( tempPath, sizeof( tempPath ) );
   strcat( tempPath, "BlockStreamer_test" );
   g_testFilePath = tempPath;

   g_data.resize( 1024 );
   std::iota( g_data.begin(), g_data.end(), 0 );

   // Write partial data and read it back.
   {
      writeDataToTempFile( g_data.data(), 5 );
      hop::BlockStreamView<unsigned, 10> bsview( 16, g_testFilePath );
      uint32_t valueRead      = bsview.readBlock( 0, 1 );
      assert( valueRead == 5 );
   }

   // Write full block and read it back
   {
      writeDataToTempFile( g_data.data(), 10 );
      hop::BlockStreamView<unsigned, 10> bsview( 16, g_testFilePath );
      uint32_t valueRead      = bsview.readBlock( 0, 1 );
      assert( valueRead == 10 );
   }

   testIterators<1>();
   testIterators<5>();
   testIterators<7>();
   testIterators<10>();
   testIterators<13>();
   testIterators<20>();
   testIterators<32>();
   testIterators<50>();
   testIterators<64>();
}