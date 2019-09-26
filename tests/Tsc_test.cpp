#include "TestUtils.h"

#include "Utils.h"

int main()
{
   HOP_TEST_ASSERT( hop::supportsRDTSCP() );
   HOP_TEST_ASSERT( hop::supportsConstantTSC() );
}
