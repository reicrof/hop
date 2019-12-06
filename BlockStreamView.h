#ifndef HOP_BLOCK_STREAM_VIEW_H_
#define HOP_BLOCK_STREAM_VIEW_H_

#include "BlockStreamer.h"

#include <cassert>
#include <iterator>
#include <utility>
#include <cstdlib>  // div
#include <memory>
#include <iostream>

namespace hop
{
template <typename T, unsigned BLK_SIZE>
class BlockStreamView
{
   using LiveBlocks = std::vector<std::unique_ptr<StreamedBlock<T, BLK_SIZE>>>;

  public:
   BlockStreamView( uint32_t liveBlockCount, const char* pathToStream )
   {
      _liveBlocks.reserve( liveBlockCount );
      _streamedFile = fopen( pathToStream, "rb" );
      if( !_streamedFile )
      {
         printf( "Cannot open temporary file %s\n", strerror( errno ) );
      }
   }

   // Returns the total number of elements read
   uint32_t readBlock( uint32_t blockOffset, uint32_t blockCount )
   {
      _liveBlocks.clear();
      uint32_t elementsRead = 0;
      for( uint32_t i = 0; i < blockCount; ++i )
      {
         _liveBlocks.push_back( std::make_unique<StreamedBlock<T, BLK_SIZE>>() );
         elementsRead += _liveBlocks.back()->read(
             _streamedFile, ( blockOffset + i ) * BLK_SIZE * sizeof( T ) );
      }

      return elementsRead;
   }

   uint32_t size() const
   {
      const size_t blockCount = _liveBlocks.size();
      if( blockCount == 0 ) return 0;

      const uint32_t lastBlockElemCount = _liveBlocks.back()->elementCount;
      if( blockCount == 1 ) return lastBlockElemCount;

      return ( blockCount - 1 ) * BLK_SIZE + lastBlockElemCount;
   }

   ~BlockStreamView()
   {
      if( fclose( _streamedFile ) != 0 )
      {
         fprintf( stderr, "Error closing file: %s\n", strerror( errno ) );
      }
   }

   /**
    * Iterator Implementation
    */
   // clang-format off
   class iterator : public std::iterator<std::forward_iterator_tag, T>
   {
      LiveBlocks* _liveBlocks;
      uint32_t _blockId;
      uint32_t _elementId;
   public:
      using difference_type = typename std::iterator<std::random_access_iterator_tag, T>::difference_type;
   
      iterator( LiveBlocks* lb ) : _liveBlocks( lb ), _blockId( 0 ), _elementId( 0 ) {}
      iterator( const iterator& rhs ) : _liveBlocks( rhs._liveBlocks ), _blockId( rhs._blockId ), _elementId( rhs._elementId ) {}
      iterator( LiveBlocks* lb, uint32_t blockId, uint32_t elId ) : _liveBlocks( lb ), _blockId( blockId ), _elementId( elId ) {}
      inline bool operator==(const iterator& rhs) const { return _blockId == rhs._blockId && _elementId == rhs._elementId; }
      inline bool operator!=(const iterator& rhs) const { return _blockId != rhs._blockId || _elementId != rhs._elementId; }
      inline T& operator*() const
      {
         assert( _blockId < _liveBlocks->size() && _elementId < BLK_SIZE );
         return (*_liveBlocks)[_blockId]->data[_elementId];
      }
      inline T* operator->() const
      {
         assert( _blockId < _liveBlocks->size() && _elementId < BLK_SIZE );
         return &(*_liveBlocks)[_blockId]->data[_elementId];
      }

      inline bool operator>(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) > std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator<(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) < std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator>=(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) >= std::make_pair(rhs._blockId, rhs._elementId);  }
      inline bool operator<=(const iterator& rhs) const { return std::make_pair(_blockId, _elementId) <= std::make_pair(rhs._blockId, rhs._elementId);  }

      // Non const operators
      inline iterator& operator++()
      {
         if( ++_elementId >= BLK_SIZE )
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
            _elementId = BLK_SIZE - 1;
         }
         return *this;
      }
      inline iterator operator--(int) { iterator tmp(*this); operator--(); return tmp; }

      inline iterator& operator+=( difference_type val )
      {
         auto divRes = ::div( val, BLK_SIZE );
         _blockId   += divRes.quot;
         _elementId += divRes.rem;
         return *this;
      }

      inline iterator& operator-=( difference_type val )
      {
         auto divRes = ::div( val, BLK_SIZE );
         assert( (int)_blockId >= divRes.quot && (int)_elementId >= divRes.rem );
         _blockId   -= divRes.quot;
         _elementId -= divRes.rem;
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

   iterator begin() { return iterator( &_liveBlocks ); }
   iterator end() { return iterator( &_liveBlocks, _liveBlocks.size(), 0 ); }

   friend std::ostream& operator<<( std::ostream& out, const BlockStreamView& bsv )
   {
      std::cout << "\n";
      for( const auto& block : bsv._liveBlocks )
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
   LiveBlocks _liveBlocks;
   FILE* _streamedFile;
};

}  // namespace hop

#endif  // HOP_BLOCK_STREAM_VIEW_H_
