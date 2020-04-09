#ifndef HOP_BLOCK_ALLOCATOR_H_
#define HOP_BLOCK_ALLOCATOR_H_

#include <cstdint>

static constexpr uint32_t HOP_BLK_SIZE_BYTES  = 512 * 1024ULL - sizeof( void* );

namespace hop
{
namespace block_allocator
{
   void initialize( uint64_t vmAllocSize );
   uint32_t blockSize();
   void* acquire();
   void release( void** block, uint32_t count );
   void terminate();
} // namespace block_allocator
} // namespace hop

#endif // HOP_BLOCK_ALLOCATOR_H_
