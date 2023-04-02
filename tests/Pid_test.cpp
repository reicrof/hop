#include "TestUtils.h"
#include "common/platform/Platform.h"
#include <string.h>

int main()
{
   printf("Testing pid...\n");
   using namespace hop;
   ProcessesInfo infos = getProcessInfoFromProcessName( "Pid_test" );
   HOP_TEST_ASSERT( infos.count == 1 );
   ProcessInfo procInfoName = infos.infos[0];
   ProcessInfo procInfoPid = getProcessInfoFromPID( procInfoName.pid );
   HOP_TEST_ASSERT( procInfoPid.pid == procInfoName.pid );

   printf("Testing pid success\n");
}