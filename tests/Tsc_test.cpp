#include "TestUtils.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void cpuid( int reg[4], int fctId )
{
#if defined(_MSC_VER)
   __cpuid( reg, fctId );
#else
   asm volatile
      ("cpuid" : "=a" (reg[0]), "=b" (reg[1]), "=c" (reg[2]), "=d" (reg[3])
       : "a" (fctId), "c" (0));
#endif   
}

static bool supportsRDTSCP()
{
   int reg[4];
   cpuid( reg, 0x80000001 );
   return reg[3] & (1 << 27);
}

static bool supportsInvariantTSC()
{
   int reg[4];
   cpuid( reg, 0x80000007 );
   return reg[3] & (1 << 8);
}

int main()
{
   HOP_TEST_ASSERT( supportsRDTSCP() );
   HOP_TEST_ASSERT( supportsInvariantTSC() );
}
