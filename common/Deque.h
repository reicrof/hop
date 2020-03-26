#ifndef HOP_DEQUE_H_
#define HOP_DEQUE_H_

#include "common/BlockAllocator.h"

#include <array>
#include <algorithm>
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
   struct Block;
  public:
   static constexpr uint32_t COUNT_PER_BLOCK = (HOP_BLK_SIZE_BYTES - sizeof(uint32_t)) / sizeof( T );
   using value_type = T;
    /**
    * Iterator Implementation
    */
   // clang-format off
   template< bool Const = false >
   class iterator : public std::iterator<std::random_access_iterator_tag, T>
   {
     public:
      using value_type = T;
      using reference  = T&;
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
      inline reference operator[]( size_t idx ) { return *(this->operator+( idx ) ); }
      inline reference operator*() const
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
         const difference_type newVal = val + _elementId; 
         _blockId   += newVal / COUNT_PER_BLOCK;
         _elementId =  newVal % COUNT_PER_BLOCK;
         return *this;
      }

      inline iterator& operator-=( difference_type val )
      {
         const uint32_t quot = val / COUNT_PER_BLOCK;
         const uint32_t rem  = val % COUNT_PER_BLOCK;
         assert( _blockId >= quot );
         _blockId -= quot;
         if( rem > _elementId )
         {
            assert( _blockId > 0 );
            --_blockId;
            _elementId = COUNT_PER_BLOCK - rem;
         }
         else
         {
            _elementId -= rem;
         }
         return *this;
      }

      inline iterator operator+( difference_type val ) const
      {
         iterator tmp(*this);
         tmp.operator+=(val);
         return tmp;
      }

      inline iterator operator-( difference_type val ) const
      {
         iterator tmp(*this);
         tmp.operator-=(val);
         return tmp;
      }

      inline difference_type operator-( const iterator& it2 ) const
      {
         difference_type diff = (difference_type)_blockId - (difference_type)it2._blockId;
         diff *= COUNT_PER_BLOCK;
         diff += (difference_type)_elementId - (difference_type)it2._elementId;
         return diff;
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

   Deque( const Deque& rhs )
   {
      this->operator=( rhs );
   }

   Deque& operator=( const Deque& rhs )
   {
      const int32_t originalSize = (int32_t)_blocks.size();
      const int32_t newSize      = (int32_t)rhs._blocks.size();
      const int32_t deltaBlocks  = originalSize - newSize; 
      if( deltaBlocks > 0 )
      {
         // Release unused blocks
         releaseBlocks( _blocks.begin() + newSize, _blocks.end() );
      }
      else
      {
         // Allocate new blocks
         const uint32_t newBlockCount = newSize - originalSize;
         for( uint32_t i = 0; i < newBlockCount; ++i )
            acquireNewBlock();
      }
      
      assert( _blocks.size() == rhs._blocks.size() );

      // Copy the data
      for( int32_t i = 0; i < newSize; ++i )
         *_blocks[i] = *rhs._blocks[i];

      return *this;
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
      return (*_blocks[idx/COUNT_PER_BLOCK])[idx%COUNT_PER_BLOCK];
   }

   T& operator[]( int64_t idx )
   {
      return (*_blocks[idx/COUNT_PER_BLOCK])[idx%COUNT_PER_BLOCK];
   }

   T& front()
   {
      assert( !_blocks.empty() && _blocks.front()->elementCount > 0 );
      Block* firstBlock = _blocks.front();
      return (*firstBlock)[ 0 ];
   }

   const T& front() const
   {
      assert( !_blocks.empty() && _blocks.front()->elementCount > 0 );
      Block* firstBlock = _blocks.front();
      return (*firstBlock)[ 0 ];
   }

   T& back()
   {
      assert( !_blocks.empty() && _blocks.back()->elementCount > 0 );
      Block* lastBlock = _blocks.back();
      return (*lastBlock)[ lastBlock->elementCount - 1 ];
   }

   const T& back() const
   {
      assert( !_blocks.empty() && _blocks.back()->elementCount > 0 );
      const Block* lastBlock = _blocks.back();
      return (*lastBlock)[ lastBlock->elementCount - 1 ];
   }

   void push_back( const T& value ) { append( &value, 1 ); }
   void append( const T& value ) { append( &value, 1 ); }
   void append( uint32_t count, const T& value )
   {
      for( uint32_t i = 0; i < count; ++i )
      {
         append( &value, 1 );
      }
   }
   void append( const T* const data, uint32_t count )
   {
      if( _blocks.empty() ) acquireNewBlock();

      uint32_t remainingWrite = count;
      const T* inData         = data;
      do
      {
         const uint32_t newRemainingWrite = _blocks.back()->append( inData, remainingWrite );
         if( newRemainingWrite > 0 )
         {
            // Block is full, acquire a new one
            acquireNewBlock();
            inData += remainingWrite - newRemainingWrite;
         }
         remainingWrite = newRemainingWrite;
      } while( remainingWrite > 0 );
   }

   template< bool Const = false >
   void append( const Deque<T>::iterator<Const>& begin, const Deque<T>::iterator<Const>& end )
   {
      assert( begin._blocks == end._blocks );
      if( begin == end ) return;

      const std::vector<Block*>* inBlocks = begin._blocks;
      uint32_t blkId = begin._blockId;
      uint32_t elId  = begin._elementId;
      for( ; blkId < end._blockId; ++blkId )
      {
         const Block* curBlock = (*inBlocks)[blkId];
         append( &curBlock->data[elId], COUNT_PER_BLOCK - elId );
         elId = 0; // From now on, we copy whole blocks
      }

      // Copy incomplete block. We need to take into account the fact that the last
      // iterator is pointing to a block that might not yet be allocated (one past
      // the end)
      if( blkId < inBlocks->size() )
      {
         Block* curBlock = (*inBlocks)[blkId];
         assert( end._elementId >= elId );
         append( &curBlock->data[elId], end._elementId - elId );
      }
   }

   void append( const T* begin, const T* end )
   {
      assert( end > begin );
      const ptrdiff_t elementCount = end - begin;
      append( begin, elementCount );
   }

   template< bool Const = false >
   void erase( const Deque<T>::iterator<Const>& el )
   {
      erase( el, el + 1 );
   }

   void erase( Deque<T>::iterator<false> from, Deque<T>::iterator<false> to )
   {
      assert( from._blockId <= to._blockId );

      /* [XXXXXX----] [----------] [----XXXXXX] -> [XXXXXX----] [----XXXXXX]*/
      eraseEntireBlocks( from, to );

      /* [XXXXXX----] [----XXXXXX] -> [xxxxxxxxxX] [--------XX] */
      erase2BlockSpans( from, to );

      /* [xxxxxxxxxX] [--------XX] -> [xxxxxxxxxX] [XX--------] ... until fully propagated */
      eraseWithinSingleBlock( from, to );
   }

   void clear()
   {
      if( _blocks.size() > 0 )
      {
         block_allocator::release( (void**)_blocks.data(), _blocks.size() );
         _blocks.clear();
      }
   }

   ~Deque()
   {
      clear();
   }

   auto begin() const { return iterator<true>( &_blocks ); }
   auto begin() { return iterator<false>( &_blocks ); }
   auto cbegin() const { return iterator<true>( &_blocks ); }
   auto end() const
   {
      const uint64_t sz = size();
      return iterator<true>( &_blocks, sz / COUNT_PER_BLOCK, sz % COUNT_PER_BLOCK );
   }
   auto end()
   {
      const uint64_t sz = size();
      return iterator<false>( &_blocks, sz / COUNT_PER_BLOCK, sz % COUNT_PER_BLOCK );
   }
   auto cend() const
   {
      const uint64_t sz = size();
      return iterator<true>( &_blocks, sz / COUNT_PER_BLOCK, sz % COUNT_PER_BLOCK );
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

    template< typename IT >
    void releaseBlocks( const IT& from, const IT& to )
    {
       block_allocator::release( (void**)&(*from), std::distance( from, to ) );
       _blocks.erase( from, to );
    }

    /* Rotate the block containing the element to leave emtpy space at the end
          [XXXX----XX] -> [XXXXXX----]
       */
    void eraseWithinSingleBlock(
        const Deque<T>::iterator<false>& from,
        const Deque<T>::iterator<false>& to )
    {
       const uint32_t removedElCount = std::distance( from, to );

       // Assert we are in the same block
       assert( from._elementId + removedElCount <= COUNT_PER_BLOCK );

       uint32_t curBlkId = from._blockId;
       if( curBlkId < _blocks.size() )
       {
          Block* curBlock = _blocks[curBlkId];
          std::rotate(
              &curBlock->data[from._elementId],
              &curBlock->data[from._elementId] + removedElCount,
              &curBlock->data[0] + COUNT_PER_BLOCK );
          while( curBlkId++ < _blocks.size() - 1 )
          {
             Block* prevBlock = curBlock;
             curBlock         = _blocks[curBlkId];
             // Copy content over to the previous block
             std::copy(
                 &curBlock->data[0],
                 &curBlock->data[0] + removedElCount,
                 &prevBlock->data[0] + COUNT_PER_BLOCK - removedElCount );
             // Update cur block
             std::rotate(
                 &curBlock->data[0],
                 &curBlock->data[0] + removedElCount,
                 &curBlock->data[0] + COUNT_PER_BLOCK );
          }

          assert( curBlock->elementCount >= removedElCount );
          // If it so happens that we removed all elements of the last block, let's release it.
          curBlock->elementCount -= removedElCount;
          if( curBlock->elementCount == 0 )
          {
             releaseBlocks( _blocks.end()-1, _blocks.end() );
          }
       }
    }

    /* Handle case where the range to remove spans 2 blocks. If so, collapse the data from the right block
     * into the left one to get back to the single block scenario
     * [XXXXXX----] [-----XXXXX] -> [xxxxxxxxxX] [---------X]
     */
    void erase2BlockSpans( Deque<T>::iterator<false>& from, Deque<T>::iterator<false>& to )
    {
       uint32_t removedElCount = std::distance( from, to );
       if( const bool multiBlock = from._elementId + removedElCount > COUNT_PER_BLOCK )
       {
          Block* leftBlk  = _blocks[from._blockId];
          Block* rightBlk = _blocks[to._blockId];

          /* Empty spaces in the left block after removal */
          uint32_t emptyLeftCnt = COUNT_PER_BLOCK - from._elementId;
          /* Remaining valid values in the right block to be copied into left block */
          const uint32_t validRightCnt = rightBlk->elementCount - to._elementId;

          T* const copyDst          = &leftBlk->data[from._elementId];
          T* const copyFrom         = &rightBlk->data[to._elementId];
          const uint32_t elemCopied = std::min( emptyLeftCnt, validRightCnt );
          T* const copyUntil        = copyFrom + elemCopied;

          // Copy data from right block into left one
          std::copy( copyFrom, copyUntil, copyDst );

          // If there was more empty slot in the left block than valid one in the right
          // continue filling from the next block to the right, if any and remove rightblock
          if( emptyLeftCnt >= validRightCnt )
          {
             // Erase the now empty right block front the list
             releaseBlocks( _blocks.begin() + to._blockId, _blocks.begin() + to._blockId + 1 );
             removedElCount -= elemCopied;

             // Fill empty slots with next blocks, if any
             emptyLeftCnt -= validRightCnt;
             if( _blocks.size() > to._blockId && emptyLeftCnt > 0 )
             {
                std::copy(
                    &_blocks[to._blockId]->data[0],
                    &_blocks[to._blockId]->data[emptyLeftCnt],
                    copyDst + validRightCnt );
             }
             else
             {
                assert( emptyLeftCnt < COUNT_PER_BLOCK );
                // There is nothing left to copy/move. Adjust the leftBlk
                // element count, and modify the from to signal we are done
                leftBlk->elementCount = COUNT_PER_BLOCK - emptyLeftCnt;
                from = to;
                return;
             }

             // Updating the iterator will setup us in the "normal" use case
             from._blockId   = to._blockId;
             from._elementId = 0;
             to              = from + emptyLeftCnt;
          }
          else
          {
             // Updating the iterator will setup us in the "normal" use case
             from._blockId   = to._blockId;
             from._elementId = 0;
             to              = to + emptyLeftCnt;
          }
       }
    }

    /*
     * Handle erasure of "full blocks"
     * [XXXXXX----] [----------] [-----XXXXX]-> [XXXXXX----] [-----XXXXX]
     */
    void eraseEntireBlocks(
        Deque<T>::iterator<false>& from,
        Deque<T>::iterator<false>& to )
    {
       const int32_t fullBlocksToRemove = to._blockId - from._blockId;
       if( fullBlocksToRemove > 1 )
       {
          // Special case where we can remove entire block
          auto removeFrom = _blocks.begin() + (from._blockId + 1);
          auto removeTo   = removeFrom + (fullBlocksToRemove - 1);
          releaseBlocks( removeFrom, removeTo );
          to._blockId -= fullBlocksToRemove - 1;
       }
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