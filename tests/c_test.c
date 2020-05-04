#include <stdlib.h>

#define HOP_IMPLEMENTATION
#include <Hop.h>

static int g_fakelock;

void fakeLock( void* lock )
{
    HOP_ACQUIRE_LOCK( lock );
    HOP_LOCK_ACQUIRED();
}

int main()
{
    HOP_INTIALIZE();
    for( int i = 0; i < 100; ++i )
    {
       HOP_ENTER_FUNC( 0 );
       HOP_ENTER("TESSST", 0);
       HOP_LEAVE();

       fakeLock( (void*) &g_fakelock );

       HOP_LEAVE();
    }

    HOP_SHUTDOWN();
}