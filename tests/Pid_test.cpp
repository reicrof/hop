#include "TestUtils.h"
#include "common/platform/Platform.h"
#include <string.h>

int main()
{
   printf("Testing pid...\n");
   using namespace hop;
   ProcessInfo procInfoName = getProcessInfoFromProcessName( "Pid_test" );
   ProcessInfo procInfoPid = getProcessInfoFromPID( procInfoName.pid );
   HOP_TEST_ASSERT( procInfoPid.pid == procInfoName.pid );
   HOP_TEST_ASSERT( strcmp( procInfoPid.name, procInfoName.name ) == 0 );

   printf("Testing pid success\n");
}