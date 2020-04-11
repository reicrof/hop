#include <stdlib.h>

#include "hop_c.h"

int main()
{
    hop_initialize();
    hop_core_t core;
    for( int i = 0; i < 100; ++i )
    {
        hop_get_timestamp( &core );
    }

    hop_terminate();
}