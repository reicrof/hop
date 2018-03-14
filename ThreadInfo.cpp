#include "ThreadInfo.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>

namespace hop
{
ThreadInfo::ThreadInfo() { traces.reserve( 2048 ); }

void ThreadInfo::addTraces( const DisplayableTraces& newTraces )
{
   traces.append( newTraces );

   assert_is_sorted( traces.ends.begin(), traces.ends.end() );
}

void ThreadInfo::addLockWaits( const std::vector<LockWait>& lockWaits )
{
   _lockWaits.insert( _lockWaits.end(), lockWaits.begin(), lockWaits.end() );
}

void ThreadInfo::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

std::vector< char > serialize( const ThreadInfo& threadInfo )
{
   std::vector< char > data;

   return data;
}

}