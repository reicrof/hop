#include "common/BlockAllocator.h"

#include "common/platform/Platform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h> // memset
#include <mutex>
#include <vector>

struct Allocator
{
   uint64_t blkSize = 0;
   std::vector<void*> _freeBlocks;
   std::vector<void*> _allocations;
   std::mutex mutex;
} g_allocator;

static constexpr uint64_t VIRT_MEM_BLK_SIZE = 1024 * 1024 * 1024ULL;
static constexpr uint32_t BLK_COUNT         = VIRT_MEM_BLK_SIZE / HOP_BLK_SIZE_BYTES;

static void allocateBlocks( Allocator* alloc )
{
   void* newAlloc = hop::virtualAlloc( VIRT_MEM_BLK_SIZE );
   alloc->_allocations.push_back( newAlloc );

   const size_t prevSize = alloc->_freeBlocks.size();
   alloc->_freeBlocks.resize( prevSize + BLK_COUNT );
   for (uint32_t i = 0; i < BLK_COUNT; ++i)
        alloc->_freeBlocks[prevSize + i] = (unsigned char*)newAlloc + (i * HOP_BLK_SIZE_BYTES);
}

namespace hop
{

namespace block_allocator
{

void initialize()
{
   assert( g_allocator._freeBlocks.empty() && g_allocator._allocations.empty() ); // Make sure we only init once

   g_allocator._allocations.reserve( 16 );
   g_allocator._freeBlocks.reserve( BLK_COUNT );

   allocateBlocks( &g_allocator );
}

uint32_t blockSize()
{
   return HOP_BLK_SIZE_BYTES;
}

void* acquire()
{
   std::lock_guard< std::mutex > g( g_allocator.mutex );
   if( g_allocator._freeBlocks.empty() )
      allocateBlocks( &g_allocator );

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
      memset( block[i], 42, HOP_BLK_SIZE_BYTES );
#endif
   g_allocator._freeBlocks.insert( g_allocator._freeBlocks.end(), block, block + count );
}

void terminate()
{
   for (void* a : g_allocator._allocations)
      hop::virtualFree( a );

   g_allocator._allocations.clear();
   g_allocator._freeBlocks.clear();
}

} // namespace block_allocator

} // namespace hop