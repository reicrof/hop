#ifndef HOP_ARRAY_H_
#define HOP_ARRAY_H_

#include "common/BlockAllocator.h"

#include <assert.h>

namespace hop
{
template <typename T>
struct Array
{
   Array()
   {
      _count = 0;
      _data  = (T*)block_allocator::acquire();
   }
   ~Array() { block_allocator::release( (void**)&_data, 1 ); }

   uint32_t size() const { return _count; }

   T& operator[]( size_t idx )
   {
      assert( idx < _count );
      return _data[idx];
   }

   const T& operator[]( size_t idx ) const
   {
      assert( idx < _count );
      return _data[idx];
   }

   void push_back( const T& t )
   {
      assert( _count < HOP_BLK_SIZE_BYTES / sizeof( T ) );
      _data[_count++] = t;
   }

   void erase( uint32_t from, uint32_t to )
   {
      assert( to - from > 0 );
      const uint32_t removeCount = to - from;
      if( removeCount == _count )
      {
         _count = 0;
      }
      else
      {
         std::rotate( _data[from], _data[to], _data[_count] );
         assert( _count >= removeCount );
         _count -= removeCount;
      }
   }

   void clear() { _count = 0; }

   T* begin() { return &_data[0]; }
   T* end() { return &_data[_count - 1]; }

   T& front() { return _data[0]; }
   const T& front() const { return _data[0]; }

   T& back() { return _data[_count - 1]; }
   const T& back() const { return _data[_count - 1]; }

   T* data() { return &_data[0]; }
   const T* data() const { return &_data[0]; }

  private:
   T* _data;
   uint32_t _count;
};

}  // namespace hop

#endif  // HOP_ARRAY_H