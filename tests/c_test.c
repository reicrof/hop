#include <stdlib.h>

#include "hop_c.h"

static int g_fakelock;

void fakeLock( void* lock )
{
    HOP_ACQUIRE_LOCK( lock );
    HOP_LOCK_ACQUIRED();
}

int main()
{
    HOP_INTIALIZE();
    hop_core_t core;
    for( int i = 0; i < 100; ++i )
    {
       HOP_ENTER_FUNC();
       HOP_ENTER("TESSST");
       HOP_LEAVE();

       fakeLock( (void*) &g_fakelock );

       HOP_LEAVE();
    }

    HOP_SHUTDOWN();
}