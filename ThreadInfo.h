#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_

#include "Hop.h"
#include "DisplayableTraces.h"

#include <vector>

namespace hop
{

struct ThreadInfo
{
   ThreadInfo();
   void addTraces( const DisplayableTraces& traces );
   void addLockWaits( const std::vector<LockWait>& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   DisplayableTraces traces;
   std::vector<LockWait> _lockWaits;
   std::vector<UnlockEvent> _unlockEvents;
};
}

#endif  // THREAD_INFO_H_
