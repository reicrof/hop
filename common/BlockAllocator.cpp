#include "common/BlockAllocator.h"

#include <assert.h>
#include <stdlib.h>
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

namespace hop
{

namespace block_allocator
{

void initialize( uint64_t blockSize, uint64_t startingBlockCount /*= 128*/ )
{
    assert( blockSize > 8 && g_allocator.blkSize == 0 ); // Make sure we only init once

    g_allocator.blkSize = blockSize;
    g_allocator._allocations.reserve( 32 );
    g_allocator._freeBlocks.reserve( 1024 );

    void* newAlloc = malloc(startingBlockCount * blockSize);
    g_allocator._allocations.push_back( newAlloc );

    const size_t prevSize = g_allocator._freeBlocks.size();
    g_allocator._freeBlocks.resize( prevSize + startingBlockCount );
    for (uint32_t i = 0; i < startingBlockCount; ++i)
       g_allocator._freeBlocks[prevSize + i] = (unsigned char*)newAlloc + (i * g_allocator.blkSize);
}

void* acquire()
{
    // std::lock_guard< std::mutex > g( g_allocator.mutex );
    // void* element = g_allocator.blocks[0];
    // g_allocator.blocks[0]
    // bd->items[0].next = bd->items[slot].next;
    // // If the freelist is empty, slot will be 0, because the header
    // // item will point to itself.
    // if (slot) return slot;
    // bd->items.resize(bd->items.size() + 1);
    // return bd->items.size() - 1;

    return nullptr;
}

void release( void* block )
{
    std::lock_guard< std::mutex > g( g_allocator.mutex );

}

void terminate()
{
    // Need to free all of the allocs, not only the first one
    assert( false );
}

} // namespace block_allocator

} // namespace hop