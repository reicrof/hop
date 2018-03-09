#include <pthread.h>
#include <Hop.h>

/*
 *  Code from
 * https://opensource.apple.com/source/dyld/dyld-210.2.3/include/mach-o/dyld-interposing.h
 * Example:
 *
 *  static
 *  int
 *  my_open(const char* path, int flags, mode_t mode)
 *  {
 *    int value;
 *    // do stuff before open (including changing the arguments)
 *    value = open(path, flags, mode);
 *    // do stuff after open (including changing the return value(s))
 *    return value;
 *  }
 *  DYLD_INTERPOSE(my_open, open)
 */

#define DYLD_INTERPOSE( _replacement, _replacee )                                    \
   __attribute__( ( used ) ) static struct                                           \
   {                                                                                 \
      const void* replacement;                                                       \
      const void* replacee;                                                          \
   } _interpose_##_replacee __attribute__( ( section( "__DATA,__interpose" ) ) ) = { \
       (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee};

/* 

Since the dyld uses pthreads to load functions, we cannot use the dlsym
to load the libary as we do in the linux version. We instead use a
mechanism specific for mac.

/!\ Hack Alert : Both wrappers uses a hack to prevent sending event
    to Hop to soon. When initializing the app, the pthread lock/unlocks
    can be called before the static variable in the Hop.h have been
    initialized. A fix might be to wrap the static variable inside
    a function to have them intialize as needed, but for now, this
    will do. The 200 value was simply a guess and might need to be
    changed
*/

static int lockCount = 0;
int hop_pthread_mutex_lock( pthread_mutex_t* mutex )
{
   if ( lockCount < 200 )
   {
      ++lockCount;
      return pthread_mutex_lock( mutex );
   }

   HOP_PROF_MUTEX_LOCK( mutex );

   return pthread_mutex_lock( mutex );
}

static int unlockCount = 0;
int hop_pthread_mutex_unlock( pthread_mutex_t* mutex )
{
   if ( unlockCount < 200 )
   {
      ++unlockCount;
      return pthread_mutex_unlock( mutex );
   }
   HOP_PROF_MUTEX_UNLOCK( mutex );

   return pthread_mutex_unlock( mutex );
}

DYLD_INTERPOSE( hop_pthread_mutex_lock, pthread_mutex_lock )
DYLD_INTERPOSE( hop_pthread_mutex_unlock, pthread_mutex_unlock )
