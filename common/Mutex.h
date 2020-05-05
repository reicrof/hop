#ifndef HOP_MUTEX_H_
#define HOP_MUTEX_H_

#include "Hop.h"

#include <mutex>

namespace hop
{
   struct Mutex
   {
      void lock()
      {
         HOP_ACQUIRE_LOCK( &_m );
         _m.lock();
         HOP_LOCK_ACQUIRED();
      }

      void unlock()
      {
         _m.unlock();
         HOP_RELEASE_LOCK( &_m );
      }

      std::mutex _m;
   };
}

#endif // HOP_MUTEX_H_