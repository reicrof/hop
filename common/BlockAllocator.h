#ifndef HOP_BLOCK_ALLOCATOR_H_
#define HOP_BLOCK_ALLOCATOR_H_

#include <cstdint>

namespace hop
{
namespace block_allocator
{
   void initialize( uint64_t blockSize, uint64_t startingBlockCount = 128 );
   void* acquire();
   void release( void* block );
   void terminate();
} // namespace block_allocator
} // namespace hop

#endif // HOP_BLOCK_ALLOCATOR_H_
