#include "common/BlockAllocator.h"

#include "common/platform/Platform.h"

#include <assert.h>
#include <atomic>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>  // memset

struct MemoryBlock
{
   MemoryBlock* next;
};

struct BlockListHead
{
   uint64_t abaCounter;
   MemoryBlock* node;
};

struct Allocator
{
   void* _baseAlloc;                        // Base pointer of the alloc
   std::atomic<BlockListHead> _freeBlocks;  // Free list
   std::atomic<uint8_t*> _end;              // Points to the block past the last allocated one
} g_allocator;

static constexpr uint64_t VIRT_MEM_BLK_SIZE   = 30 * 1024 * 1024 * 1024ULL;
static constexpr uint32_t BLK_AND_HEADER_SIZE = HOP_BLK_SIZE_BYTES + sizeof( MemoryBlock );

// Make sure a block filled with pointers can express the whole range of allocated memory
static_assert(
    ( HOP_BLK_SIZE_BYTES / sizeof( void* ) ) * HOP_BLK_SIZE_BYTES >= VIRT_MEM_BLK_SIZE,
    "A pointer of blocks would not be able to express the whole virtual allocation" );

namespace hop
{
namespace block_allocator
{
void initialize()
{
   g_allocator._baseAlloc = hop::virtualAlloc( VIRT_MEM_BLK_SIZE );
   if( !g_allocator._baseAlloc )
   {
      fprintf( stderr, "Fatal Error : Failed to allocate enough virtual memory\n" );
      exit( 2 );
   }

   MemoryBlock* firstBlock = (MemoryBlock*)g_allocator._baseAlloc;
   g_allocator._freeBlocks.store( BlockListHead{0, firstBlock} );
   MemoryBlock* curBlockAddr = firstBlock;
   for( uint32_t i = 1; i < 512; ++i )
   {
      curBlockAddr->next = (MemoryBlock*)( (uint8_t*)curBlockAddr + BLK_AND_HEADER_SIZE );
      curBlockAddr       = curBlockAddr->next;
   }
   curBlockAddr->next = nullptr;
   g_allocator._end   = (uint8_t*)curBlockAddr + BLK_AND_HEADER_SIZE;
}

uint32_t blockSize() { return HOP_BLK_SIZE_BYTES; }

void* acquire()
{
   BlockListHead prevHead = g_allocator._freeBlocks.load();
   BlockListHead newHead;
   do
   {
      if( !prevHead.node ) break;

      newHead.abaCounter = prevHead.abaCounter + 1;
      newHead.node       = prevHead.node->next;
   } while( !g_allocator._freeBlocks.compare_exchange_weak( prevHead, newHead ) );

   // If we were not able to get one from the freelist, bump the allocation pointer
   MemoryBlock* acquiredBlk = prevHead.node
                                  ? prevHead.node
                                  : (MemoryBlock*)g_allocator._end.fetch_add( BLK_AND_HEADER_SIZE );
   acquiredBlk->next = nullptr;

   // Out of memory
   assert( (uint64_t)acquiredBlk < (uint64_t)g_allocator._baseAlloc + VIRT_MEM_BLK_SIZE );
   if( (uint64_t)acquiredBlk >= (uint64_t)g_allocator._baseAlloc + VIRT_MEM_BLK_SIZE )
   {
      printf( "No more memory. Exiting..." );
      exit( -2 );
   }

   return (uint8_t*)acquiredBlk + sizeof( MemoryBlock );
}

void release( void** block, uint32_t count )
{
   assert( count > 0 );

   // The new release blocks will become the new head
   BlockListHead newHead;
   newHead.node = (MemoryBlock*)( (uint8_t*)block[0] - sizeof( MemoryBlock ) );

   // Chain all the freed blocks together to create the new head
   MemoryBlock* curBlock = newHead.node;
   for( uint32_t i = 1; i < count; ++i )
   {
      curBlock->next = (MemoryBlock*)( (uint8_t*)block[i] - sizeof( MemoryBlock ) );
      curBlock       = curBlock->next;
   }

   BlockListHead prevHead = g_allocator._freeBlocks.load();
   do
   {
      newHead.abaCounter = prevHead.abaCounter + 1;
      curBlock->next     = prevHead.node;
   } while( !g_allocator._freeBlocks.compare_exchange_weak( prevHead, newHead ) );
}

void terminate()
{
   hop::virtualFree( g_allocator._baseAlloc, VIRT_MEM_BLK_SIZE );
   g_allocator._baseAlloc = nullptr;
}

}  // namespace block_allocator

}  // namespace hop