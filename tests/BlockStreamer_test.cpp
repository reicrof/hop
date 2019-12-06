#include "BlockStreamer.h"
#include "platform/Platform.h"

#include <cassert>
#include <numeric>
#include <vector>

const char* g_testFilePath;

template <typename T, unsigned BLK_SIZE>
static bool testBlockStreamerWrite( const T* values, uint32_t count, uint32_t iterations )
{
   {
      hop::BlockStreamer<T, BLK_SIZE> bs( g_testFilePath );
      assert( bs.valid() );
      for( uint32_t i = 0; i < iterations; ++i )
      {
         bs.append( values, count );
      }
   }

   FILE* readTestFile                    = fopen( g_testFilePath, "rb" );
   const uint32_t expectedRdCount = count * iterations;
   std::vector< T > readbackData( expectedRdCount );
   uint32_t countRead = fread( readbackData.data(), sizeof( T ), expectedRdCount, readTestFile );

   if( countRead != expectedRdCount )
      return false;


   for( uint32_t i = 0; i < expectedRdCount; ++i )
   {
      if( readbackData[i] != values[i % count] )
         return false;
   }

   fclose( readTestFile );

   return true;
}

template <typename T, unsigned BLK_SIZE>
static bool testBlockStream( uint32_t valueCount )
{
   std::vector<T> data( valueCount );
   std::iota( std::begin( data ), std::end( data ), 0 );
   for( uint32_t i = 0; i < valueCount; ++i )
   {
      for( uint32_t j = 0; j < valueCount; ++j )
      {
         if( !testBlockStreamerWrite<T, BLK_SIZE>( data.data(), i, j ) )
         {
            return false;
         }
      }
   }

   return true;
}

struct ArrayStruct
{
   ArrayStruct()
   {
      std::iota( data.begin(), data.end(), -16 );
   }
   bool operator!=( const ArrayStruct& other )
   {
      return data != other.data;
   }
   std::array< int, 16 > data;
};

int main()
{
   // Init temp file path first
   char tempPath[512];
   hop::getTempFolderPath( tempPath, sizeof(tempPath) );
   strcat( tempPath, "BlockStreamer_test" );
   g_testFilePath = tempPath;

   bool success = false;
   success = testBlockStream<unsigned char, 1>( 16 );
   assert( success );
   success = testBlockStream<unsigned char, 8>( 32 );
   assert( success );
   success = testBlockStream<unsigned char, 25>( 64 );
   assert( success );
   success = testBlockStream<unsigned, 10>( 64 );
   assert( success );
   success = testBlockStream<float, 10>( 64 );
   assert( success );

   // Test with struct containing an array
   std::vector< ArrayStruct > arrays( 32 );
   success = testBlockStreamerWrite<ArrayStruct, 10>( arrays.data(), 4, 8 );
   assert( success );
}