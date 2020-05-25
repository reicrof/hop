#include "TestUtils.h"

#include "common/Utils.h"

int main()
{
   HOP_TEST_ASSERT( hop::supportsRDTSCP() );
   HOP_TEST_ASSERT( hop::supportsConstantTSC() );
}
