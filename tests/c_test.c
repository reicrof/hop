#include <stdlib.h>

#include "hop_c.h"

int main()
{
    hop_initialize();
    hop_core_t core;
    for( int i = 0; i < 100; ++i )
    {
       HOP_ENTER_FUNC();
       HOP_ENTER("TESSST");
       HOP_LEAVE();
       HOP_LEAVE();
    }

    hop_terminate();
}