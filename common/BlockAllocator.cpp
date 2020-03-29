#include "common/BlockAllocator.h"

#include "common/platform/Platform.h"

#include "Hop.h"

#include <assert.h>
#include <atomic>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>  // memset

struct MemoryBlock
{
   MemoryBlock* next;
};

struct Allocator
{
   void* _baseAlloc;                       // Base pointer of the alloc
   std::atomic<MemoryBlock*> _freeBlocks;  // Free list
   std::atomic<uint8_t*> _end;             // Points to the block past the last allocated one
} g_allocator;

static constexpr uint64_t VIRT_MEM_BLK_SIZE   = 8 * 1024 * 1024 * 1024ULL;
static constexpr uint32_t BLK_AND_HEADER_SIZE = HOP_BLK_SIZE_BYTES + sizeof( MemoryBlock );

namespace hop
{
namespace block_allocator
{
void initialize()
{
   //g_allocator._baseAlloc = hop::virtualAlloc( VIRT_MEM_BLK_SIZE );
   g_allocator._baseAlloc = malloc( VIRT_MEM_BLK_SIZE );
   if( !g_allocator._baseAlloc )
   {
      fprintf( stderr, "Fatal Error : Failed to allocate enough virtual memory\n" );
      exit( 2 );
   }

   g_allocator._freeBlocks = (MemoryBlock*)g_allocator._baseAlloc;

   MemoryBlock* curBlockAddr = g_allocator._freeBlocks;
   for( uint32_t i = 0; i < 511; ++i )
   {
      curBlockAddr->next = (MemoryBlock*)( (uint8_t*)curBlockAddr + BLK_AND_HEADER_SIZE );
      curBlockAddr       = curBlockAddr->next;
   }
   curBlockAddr->next = nullptr;
   g_allocator._end = (uint8_t*)curBlockAddr + BLK_AND_HEADER_SIZE;
}

uint32_t blockSize() { return HOP_BLK_SIZE_BYTES; }

void* acquire()
{
   HOP_PROF_FUNC();
   MemoryBlock* availBlock = nullptr;
   MemoryBlock* root       = g_allocator._freeBlocks.load();

   while (root && !std::atomic_compare_exchange_weak(&g_allocator._freeBlocks, &root, root->next))
   {
   }

   if( root )
   {
      availBlock = root;
   }
   else
   {
      // Bump the last pointer one block further
      availBlock = (MemoryBlock*)g_allocator._end.fetch_add( BLK_AND_HEADER_SIZE );
   }

   // Out of memory
   assert( (ptrdiff_t)availBlock < (ptrdiff_t)g_allocator._baseAlloc + VIRT_MEM_BLK_SIZE );
   assert( availBlock );

   return (uint8_t*)availBlock + sizeof( MemoryBlock );
}

void release( void** block, uint32_t count )
{
   assert( count > 0 );

   MemoryBlock* blocks = (MemoryBlock*)( (uint8_t*)block[0] - sizeof( MemoryBlock ) );

   assert( ( (ptrdiff_t)blocks & 8 ) == 0 );

   // Chain all the freed blocks together
   MemoryBlock* curBlock = blocks;
   for( uint32_t i = 1; i < count; ++i )
   {
      curBlock->next = (MemoryBlock*)((uint8_t*)block[i] - sizeof( MemoryBlock ));
      curBlock       = curBlock->next;
   }

   curBlock->next = std::atomic_exchange( &g_allocator._freeBlocks, blocks );
   assert( ( (ptrdiff_t)g_allocator._freeBlocks.load() & 8 ) == 0 );
}

void terminate()
{
   //hop::virtualFree( g_allocator._baseAlloc );
   free( g_allocator._baseAlloc );
   g_allocator._baseAlloc = nullptr;
   g_allocator._freeBlocks.store( nullptr );
}

}  // namespace block_allocator

}  // namespace hop