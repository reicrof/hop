#include "ThreadInfo.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>

namespace vdbg
{
ThreadInfo::ThreadInfo() { traces.reserve( 1024 ); }

void ThreadInfo::addTraces( const DisplayableTraces& newTraces )
{
   traces.append( newTraces );

   assert_is_sorted( traces.ends.begin(), traces.ends.end() );
}

void ThreadInfo::addLockWaits( const std::vector<LockWait>& lockWaits )
{
   _lockWaits.insert( _lockWaits.end(), lockWaits.begin(), lockWaits.end() );
}

}