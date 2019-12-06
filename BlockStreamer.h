#ifndef HOP_BLOCK_STREAMER_H_
#define HOP_BLOCK_STREAMER_H_

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>

#include <array>
#include <algorithm>
#include <cassert>
#include <type_traits>
#include <vector>

#include <iostream>

namespace hop
{
template< typename T, unsigned BLK_SIZE >
struct StreamedBlock
{
   StreamedBlock() : elementCount( 0 ) {
      static_assert( std::is_trivially_copyable<T>::value, "Type is not POD" );
   }

   uint32_t append( const T* inData, uint32_t count )
   {
      const uint32_t remainingSlot = BLK_SIZE - elementCount;
      const uint32_t writeCount = std::min( count, remainingSlot );
      memcpy( data.data() + elementCount, inData, writeCount * sizeof( T ) );
      elementCount += writeCount; 
      return count - writeCount;
   }

   // Returns the number of element read
   uint32_t read( FILE* file, uint32_t offset )
   {
      fseek( file, offset, SEEK_SET );
      elementCount = fread( data.data(), sizeof( T ), BLK_SIZE, file );
      return elementCount;
   }

   std::array<T, BLK_SIZE> data;
   uint32_t elementCount;
};

template <typename T, unsigned BLK_SIZE, class Compare = std::less<T> >
class BlockStreamer
{
  public:
   BlockStreamer( const char* pathToStream ) : _currentBlockId( 0 )
   {
      _streamedFile = fopen( pathToStream, "w+b" );
      if( !_streamedFile )
      {
         printf("Cannot open temporary file %s\n", strerror(errno));
      }
   }

   bool valid() const
   {
      return _streamedFile != nullptr;
   }

   void append( const T* const inData, uint32_t count )
   {
      uint32_t remainingWrite = count;
      const T* data           = inData;
      while( ( remainingWrite = _block.append( data, remainingWrite ) ) )
      {
         // Block is full, write it to disk
         fwrite( _block.data.data(), sizeof( T ), BLK_SIZE, _streamedFile );
         data = inData + ( count - remainingWrite );  // Advance data pointer for next write
         _block.elementCount = 0;
      }
   }

   ~BlockStreamer()
   {
      // Flush patial block to disk
      std::fill( std::begin( _block.data ) + _block.elementCount, std::end( _block.data ), T{} );
      fwrite( _block.data.data(), sizeof(T), BLK_SIZE, _streamedFile );
      if( fclose( _streamedFile ) != 0 )
      {
         fprintf(stderr, "Error closing file: %s\n", strerror( errno ));
      }
   }

  private:
   StreamedBlock<T, BLK_SIZE> _block;
   FILE* _streamedFile;
   uint32_t _currentBlockId;
};

}  // namespace hop

#endif  // HOP_BLOCK_STREAMER_H_
