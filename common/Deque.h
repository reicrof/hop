#ifndef HOP_DEQUE_H_
#define HOP_DEQUE_H_

#include "common/BlockAllocator.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring> // memcpy
#include <cstddef>
#include <type_traits>
#include <iostream>
#include <vector>

namespace hop
{
template <typename T>
class Deque
{
   static constexpr uint32_t COUNT_PER_BLOCK = (HOP_BLK_SIZE_BYTES - sizeof(uint32_t)) / sizeof( T );
   struct Block;
  public:
   using value_type = T;
    /**
    * Iterator Implementation
    */
   // clang-format off
   template< bool Const = false >
   class iterator : public std::iterator<std::forward_iterator_tag, T>
   {
     public:
      using value_type = T;
      using difference_type = typename std::iterator<std::random_access_iterator_tag, T>::difference_type;
      using VectorBlocksPtr = typename std::conditional< Const, const std::vector<Block*>, std::vector<Block*> >::type;

      VectorBlocksPtr* _blocks;
      uint32_t _blockId;
      uint32_t _elementId;
   
      iterator( VectorBlocksPtr* lb ) : _blocks( lb ), _blockId( 0 ), _elementId( 0 ) {}
      iterator( const iterator& rhs ) : _blocks( rhs._blocks ), _blockId( rhs._blockId ), _elementId( rhs._elementId ) {}
      iterator( VectorBlocksPtr* lb, uint32_t blockId, uint32_t elId ) : _blocks( lb ), _blockId( blockId ), _elementId( elId ) {}
      inline bool operator==(const iterator& rhs) const { return _blockId == rhs._blockId && _elementId == rhs._elementId; }
      inline bool operator!=(const iterator& rhs) const { return _blockId != rhs._blockId || _elementId != rhs._elementId; }
      inline T& operator*() const
      {
         assert( _blockId < _blocks->size() && _elementId < COUNT_PER_BLOCK );
         return (*_blocks)[_blockId]->data[_elementId];
      }
      inline T* operator->() const
      {
         assert( _blockId < _blocks->size() && _elementId < COUNT_PER_BLOCK );
         return &(*_blocks)[_blockId]->data[_elementId];
      }

      inline bool operator>(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) > std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator<(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) < std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator>=(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) >= std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator<=(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) <= std::make_pair(rhs._blockId, rhs._elementId);  }

      // Non const operators
      inline iterator& operator++()
      {
         if( ++_elementId >= COUNT_PER_BLOCK )
         {
            _elementId = 0;
            ++_blockId;
         }
         return *this;
      }
      inline iterator operator++(int) { iterator tmp(*this); operator++(); return tmp; }
      inline iterator& operator--()
      {
         if( _elementId > 0 )
         {
            --_elementId;
         }
         else
         {
            assert( _blockId > 0 );
            --_blockId;
            _elementId = COUNT_PER_BLOCK - 1;
         }
         return *this;
      }
      inline iterator operator--(int) { iterator tmp(*this); operator--(); return tmp; }

      inline iterator& operator+=( difference_type val )
      {
         auto divRes = ::div( val + _elementId, COUNT_PER_BLOCK );
         _blockId   += divRes.quot;
         _elementId = divRes.rem;
         return *this;
      }

      inline iterator& operator-=( difference_type val )
      {
         auto divRes = ::div( val, COUNT_PER_BLOCK );
         assert( (int)_blockId >= divRes.quot );
         _blockId   -= divRes.quot;
         if( divRes.rem > (int)_elementId )
         {
            assert( _blockId > 0 );
            --_blockId;
            _elementId = COUNT_PER_BLOCK - divRes.rem;
         }
         else
         {
            _elementId -= divRes.rem;
         }
         return *this;
      }

      inline iterator operator+( difference_type val )
      {
         iterator tmp(*this);
         tmp.operator+=(val);
         return tmp;
      }

      inline iterator operator-( difference_type val )
      {
         iterator tmp(*this);
         tmp.operator-=(val);
         return tmp;
      }
   };
   // clang-format on

   /**
    * Actual Deque implementation
    */

   Deque()
   {
       assert( block_allocator::blockSize() >= HOP_BLK_SIZE_BYTES );
   }

   uint64_t size() const
   {
      const size_t blockCount = _blocks.size();
      if( blockCount == 0 ) return 0;

      const uint32_t lastBlockElemCount = _blocks.back()->elementCount;
      if( blockCount == 1 ) return lastBlockElemCount;

      return ( blockCount - 1 ) * COUNT_PER_BLOCK + lastBlockElemCount;
   }

   bool empty() const { return _blocks.empty(); }

   const T& operator[]( int64_t idx ) const
   {
      auto divRes = ::div( idx, COUNT_PER_BLOCK );
      return (*_blocks[divRes.quot])[divRes.rem];
   }

   T& operator[]( int64_t idx )
   {
      auto divRes = ::div( idx, COUNT_PER_BLOCK );
      return (*_blocks[divRes.quot])[divRes.rem];
   }

   T& back()
   {
      Block* lastBlock = _blocks.back();
      return (*lastBlock)[ lastBlock->elementCount ];
   }

   const T& back() const
   {
      const Block* lastBlock = _blocks.back();
      return (*lastBlock)[ lastBlock->elementCount ];
   }

   void append( const T& value ) { append( &value, 1 ); }
   void push_back( const T& v ) { append( &v, 1 ); }
   void append( const T* const inData, uint32_t count )
   {
      if( _blocks.empty() ) acquireNewBlock();

      uint32_t remainingWrite = count;
      const T* data           = inData;
      while( ( remainingWrite = _blocks.back()->append( data, remainingWrite ) ) )
      {
         // Block is full, acquire a new one
         acquireNewBlock();
         data += COUNT_PER_BLOCK;
      }
   }

   template< bool Const = false >
   void append( Deque<T>::iterator<Const> begin, Deque<T>::iterator<Const> end )
   {
      assert( begin._blocks == end._blocks );
      const std::vector<Block*>* inBlocks = begin._blocks;
      uint32_t blkId = begin._blockId;
      uint32_t elId  = begin._elementId;
      for( ; blkId < end._blockId; ++blkId )
      {
         const Block* curBlock = (*inBlocks)[blkId];
         append( &curBlock->data[elId], COUNT_PER_BLOCK - elId );
         elId = 0; // From now on, we copy whole blocks
      }

      // Copy incomplete block
      Block* curBlock = (*inBlocks)[blkId];
      append( &curBlock->data[elId], end._elementId );
   }

   void append( const T* begin, const T* end )
   {
      assert( end > begin );
      const ptrdiff_t elementCount = end - begin;
      append( begin, elementCount );
   }

   void clear()
   {
      block_allocator::release( (void**)_blocks.data(), _blocks.size() );
      _blocks.clear();
   }

   ~Deque()
   {
      clear();
   }

   auto begin() const { return iterator<true>( &_blocks ); }
   auto begin() { return iterator<false>( &_blocks ); }
   auto end() const
   {
       auto divRes = ::div( (int64_t )size(), (int64_t)COUNT_PER_BLOCK );
       return iterator<true>( &_blocks, divRes.quot, divRes.rem );
   }
   auto end()
   {
       auto divRes = ::div( (int64_t )size(), (int64_t)COUNT_PER_BLOCK );
       return iterator<false>( &_blocks, divRes.quot, divRes.rem );
   }

   friend std::ostream& operator<<( std::ostream& out, const Deque& bsv )
   {
      std::cout << "\n";
      for( const auto& block : bsv._blocks )
      {
         std::cout << "[";
         for( uint32_t i = 0; i < block->elementCount-1; ++i )
         {
            std::cout << block->data[i] << ", ";
         }
         std::cout << block->data[block->elementCount-1] <<"]\n";
      }
      std::cout << "\n";

      return out;
   }

  private:
    void acquireNewBlock()
    {
        Block* newBlock = (Block*) block_allocator::acquire();
        newBlock->elementCount = 0;
        _blocks.push_back( newBlock );
    }

    struct Block
    {
       Block() : elementCount( 0 ) {
#if defined(__GNUC__) && (__GNUC___ > 5)
           static_assert( std::is_trivially_copyable<T>::value, "Type is not POD" );
#endif
       }

       uint32_t append( const T* inData, uint32_t count )
       {
           const uint32_t remainingSlot = COUNT_PER_BLOCK - elementCount;
           const uint32_t writeCount = std::min( count, remainingSlot );
           memcpy( data.data() + elementCount, inData, writeCount * sizeof( T ) );
           elementCount += writeCount; 
           return count - writeCount;
       }

      inline T& operator[]( int64_t idx )
      {
         assert( idx < elementCount );
         return data[idx];
      }

      inline const T& operator[]( int64_t idx ) const
      {
         assert( idx < elementCount );
         return data[idx];
      }

       uint32_t elementCount;
       std::array<T, COUNT_PER_BLOCK> data;
    };

   std::vector<Block*> _blocks;
};

}  // namespace hop

#endif // HOP_DEQUE_H_