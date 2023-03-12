#include "TestUtils.h"

#include "common/Utils.h"

int main()
{
#ifdef __x86_64__
   HOP_TEST_ASSERT( hop::supportsConstantTSC() );
#endif
}
