#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_

#include "vdbg.h"
#include "DisplayableTraces.h"

#include <vector>

namespace vdbg
{
struct ThreadInfo
{
   ThreadInfo();
   void addTraces( const DisplayableTraces& traces );
   void addLockWaits( const std::vector<LockWait>& lockWaits );
   DisplayableTraces traces;
   std::vector<char> stringData;
   std::vector<LockWait> _lockWaits;
};
}

#endif  // THREAD_INFO_H_