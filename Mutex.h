#ifndef HOP_MUTEX_H_
#define HOP_MUTEX_H_

#include <mutex>
#include "Hop.h"

namespace hop
{
   struct Mutex
   {
      void lock()
      {
         HOP_PROF_MUTEX_LOCK( &_m );
         _m.lock();
      }

      void unlock()
      {
         _m.unlock();
         HOP_PROF_MUTEX_UNLOCK( &_m );
      }

      std::mutex _m;
   };
}

#endif // HOP_MUTEX_H_