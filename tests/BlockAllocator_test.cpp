#include "TestUtils.h"
#include "common/BlockAllocator.h"
#include <string.h>
#include <vector>

void testBlockAllocator()
{
   using namespace hop;
   for( int i = 0; i < 1025; ++i )
   {
      void* b = block_allocator::acquire();
      ((int*)b)[0] = 12; // Touch the memory
      block_allocator::release( &b, 1 );
   }

   std::vector< void* > allocs;
   for( int i = 0; i < 1025; ++i )
   {
      allocs.push_back( block_allocator::acquire() );
   }


   block_allocator::release( allocs.data(), allocs.size() );
}

int main()
{
   hop::block_allocator::initialize( 2048 * HOP_BLK_SIZE_BYTES );
   testBlockAllocator();
   hop::block_allocator::terminate();
}