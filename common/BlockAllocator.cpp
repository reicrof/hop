#include "common/BlockAllocator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h> // memset
#include <mutex>
#include <vector>

namespace
{
   struct Allocator
   {
      uint64_t blkSize = 0;
      std::vector<void*> _freeBlocks;
      std::vector<void*> _allocations;
      std::mutex mutex;
   } g_allocator;

} // anonymous namespace

static void allocateBlocks( Allocator* alloc, uint64_t blockSize, uint64_t blockCount )
{
   void* newAlloc = malloc(blockCount * blockSize);
   alloc->_allocations.push_back( newAlloc );

   const size_t prevSize = alloc->_freeBlocks.size();
   alloc->_freeBlocks.resize( prevSize + blockCount );
   for (uint32_t i = 0; i < blockCount; ++i)
        alloc->_freeBlocks[prevSize + i] = (unsigned char*)newAlloc + (i * alloc->blkSize);
}

namespace hop
{

namespace block_allocator
{

void initialize( uint64_t blockSize, uint64_t startingBlockCount /*= 128*/ )
{
   assert( blockSize > 8 && g_allocator._freeBlocks.empty() && g_allocator._allocations.empty() ); // Make sure we only init once

   g_allocator.blkSize = blockSize;
   g_allocator._allocations.reserve( 32 );
   g_allocator._freeBlocks.reserve( 1024 );

   allocateBlocks( &g_allocator, blockSize, startingBlockCount );
}

uint32_t blockSize()
{
   return g_allocator.blkSize;
}

void* acquire()
{
   assert( g_allocator.blkSize > 0 && "Allocator not initialized" );

   std::lock_guard< std::mutex > g( g_allocator.mutex );
   if( g_allocator._freeBlocks.empty() )
      allocateBlocks( &g_allocator, g_allocator.blkSize, g_allocator._allocations.size() );

   assert( !g_allocator._freeBlocks.empty()  );

   void* block = g_allocator._freeBlocks.back();
   g_allocator._freeBlocks.pop_back();
   return block;
}

void release( void** block, uint32_t count )
{
   std::lock_guard< std::mutex > g( g_allocator.mutex );
#ifdef HOP_DEBUG
   for( uint32_t i = 0; i < count; ++i )
      memset( block[i], 42, g_allocator.blkSize );
#endif
   g_allocator._freeBlocks.insert( g_allocator._freeBlocks.end(), block, block + count );
}

void terminate()
{
   for (void* a : g_allocator._allocations)
      free(a);

   g_allocator._allocations.clear();
   g_allocator._freeBlocks.clear();
}

} // namespace block_allocator

} // namespace hop