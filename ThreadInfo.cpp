#include "ThreadInfo.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace hop
{  
void ThreadInfo::addTraces( const DisplayableTraces& newTraces )
{
   _traces.append( newTraces );

   assert_is_sorted(_traces.ends.begin(), _traces.ends.end() );
}

void ThreadInfo::addLockWaits( const DisplayableLockWaits& lockWaits )
{
   _lockWaits.append( lockWaits );
}

void ThreadInfo::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

TDepth_t ThreadInfo::maxDepth() const noexcept
{
   return _traces.maxDepth;
}

size_t serializedSize( const ThreadInfo& ti )
{
   const size_t tracesCount = ti._traces.ends.size();
   const size_t serializedSize =
       sizeof( size_t ) +                            // Traces count
       sizeof( hop::TDepth_t ) +                     // Max depth
       sizeof( hop::TimeStamp ) * tracesCount +      // ends
       sizeof( hop::TimeDuration ) * tracesCount +   // deltas
       sizeof( hop::TStrPtr_t ) * tracesCount * 2 +  // fileNameId and fctNameIds
       sizeof( hop::TLineNb_t ) * tracesCount +      // lineNbs
       // sizeof( hop::TGroup_t ) * tracesCount +       // groups
       sizeof( hop::TDepth_t ) * tracesCount;  // depths

   return serializedSize;
}

size_t serialize( const ThreadInfo& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    const size_t tracesCount = ti._traces.ends.size();

    size_t i = 0;
    // Traces count
    memcpy( &data[i], &tracesCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // Max depth
    memcpy( &data[i], &ti._traces.maxDepth, sizeof( hop::TDepth_t ) );
    i += sizeof( hop::TDepth_t );

    //ends
    std::copy(ti._traces.ends.begin(), ti._traces.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy( ti._traces.deltas.begin(), ti._traces.deltas.end(), (hop::TimeDuration*)&data[i] );
    i += sizeof( hop::TimeDuration ) * tracesCount;

    // fileNameIds
    std::copy( ti._traces.fileNameIds.begin(), ti._traces.fileNameIds.end(), (hop::TStrPtr_t*)&data[i]);
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // fctNameIds
    std::copy( ti._traces.fctNameIds.begin(), ti._traces.fctNameIds.end(), (hop::TStrPtr_t*)&data[i]);
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // lineNbs
    std::copy( ti._traces.lineNbs.begin(), ti._traces.lineNbs.end(), (hop::TLineNb_t*) &data[i] );
    i += sizeof( hop::TLineNb_t ) * tracesCount;

    // // groups
    // memcpy( &data[i], ti.traces.groups.data(), sizeof( hop::TGroup_t ) * tracesCount );
    // i += sizeof( hop::TGroup_t ) * tracesCount;

    // depths
    std::copy( ti._traces.depths.begin(), ti._traces.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * tracesCount;

    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, ThreadInfo& ti )
{
    size_t i = 0;
    const size_t tracesCount = *(size_t*)&data[i];
    i += sizeof( size_t );
    ti._traces.maxDepth = *(hop::TDepth_t*)&data[i];
    i += sizeof( hop::TDepth_t );

    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + tracesCount, std::back_inserter(ti._traces.ends));
    i += sizeof( hop::TimeStamp ) * tracesCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + tracesCount, std::back_inserter(ti._traces.deltas));
    i += sizeof( hop::TimeDuration ) * tracesCount;

    // fileNameIds
    std::copy((hop::TStrPtr_t*)&data[i], ((hop::TStrPtr_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.fileNameIds));
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // fctNameIds
    std::copy((hop::TStrPtr_t*) &data[i], ((hop::TStrPtr_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.fctNameIds));
    i += sizeof( hop::TStrPtr_t ) * tracesCount;

    // lineNbs
    std::copy((hop::TLineNb_t*)&data[i], ((hop::TLineNb_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.lineNbs));
    i += sizeof( hop::TLineNb_t ) * tracesCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + tracesCount, std::back_inserter(ti._traces.depths));
    i += sizeof( hop::TDepth_t ) * tracesCount;

    return i;
}

}