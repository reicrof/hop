#include "TestUtils.h"
#include "common/BlockAllocator.h"
#include <string.h>

int main()
{
   using namespace hop;
   hop::block_allocator::initialize( 256, 1024 );
}