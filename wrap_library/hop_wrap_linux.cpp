#include <dlfcn.h>
#include <pthread.h>

#include <Hop.h>

static int ( *real_pthread_mutex_lock )( pthread_mutex_t* mutex ) = nullptr;
static int ( *real_pthread_mutex_unlock )( pthread_mutex_t* mutex ) = nullptr;

int pthread_mutex_lock( pthread_mutex_t* mutex ) noexcept
{
   if ( !real_pthread_mutex_lock )
      real_pthread_mutex_lock =
          (int ( * )( pthread_mutex_t* ))dlsym( RTLD_NEXT, "pthread_mutex_lock" );

   HOP_PROF_MUTEX_LOCK( mutex );

   return real_pthread_mutex_lock( mutex );
}

int pthread_mutex_unlock( pthread_mutex_t* mutex ) noexcept
{
   if ( !real_pthread_mutex_unlock )
      real_pthread_mutex_unlock =
          (int ( * )( pthread_mutex_t* ))dlsym( RTLD_NEXT, "pthread_mutex_unlock" );

   HOP_PROF_MUTEX_UNLOCK( mutex );

   return real_pthread_mutex_unlock( mutex );
}