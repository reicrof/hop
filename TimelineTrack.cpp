#include "TimelineTrack.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace hop
{  
void TimelineTrack::addTraces( const DisplayableTraces& newTraces )
{
   _traces.append( newTraces );

   assert_is_sorted(_traces.ends.begin(), _traces.ends.end() );
}

void TimelineTrack::addLockWaits( const DisplayableLockWaits& lockWaits )
{
   _lockWaits.append( lockWaits );
}

void TimelineTrack::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

TDepth_t TimelineTrack::maxDepth() const noexcept
{
   return _traces.maxDepth;
}

float TimelineTrack::maxDisplayedDepth() const noexcept
{
   return std::min( (float)_traces.maxDepth, _trackHeight ) + 1.0f;
}

void TimelineTrack::setTrackHeight( float height )
{
   _trackHeight = hop::clamp( height, -1.0f, (float)maxDepth() );
}

bool TimelineTrack::empty() const
{
   return _traces.ends.empty();
}

size_t serializedSize( const TimelineTrack& ti )
{
   const size_t tracesCount = ti._traces.ends.size();
   const size_t lockwaitsCount = ti._lockWaits.ends.size();
   const size_t serializedSize =
       // Traces
       sizeof( size_t ) +                            // Traces count
       sizeof( hop::TDepth_t ) +                     // Max depth
       sizeof( hop::TimeStamp ) * tracesCount +      // ends
       sizeof( hop::TimeDuration ) * tracesCount +   // deltas
       sizeof( hop::TStrPtr_t ) * tracesCount * 2 +  // fileNameId and fctNameIds
       sizeof( hop::TLineNb_t ) * tracesCount +      // lineNbs
       sizeof( hop::TZoneId_t ) * tracesCount +      // zones
       sizeof( hop::TDepth_t ) * tracesCount +       // depths

       // Lock Waits
       sizeof( size_t ) +                            // LockWaits count
       sizeof( hop::TimeStamp ) * lockwaitsCount +   // ends
       sizeof( hop::TimeDuration ) * lockwaitsCount +// deltas
       sizeof( hop::TDepth_t ) * lockwaitsCount +    // depths
       sizeof( void* ) * lockwaitsCount;             // mutexAddrs

   return serializedSize;
}

size_t serialize( const TimelineTrack& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    size_t i = 0;

    // Serialize Traces
    {
    // Traces count
    const size_t tracesCount = ti._traces.ends.size();
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

    // zones
    std::copy( ti._traces.zones.begin(), ti._traces.zones.end(), (hop::TZoneId_t*) &data[i] );
    i += sizeof( hop::TZoneId_t ) * tracesCount;

    // depths
    std::copy( ti._traces.depths.begin(), ti._traces.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Serialize LockWaits
    {
    // LockWaits count
    const size_t lockwaitsCount = ti._lockWaits.ends.size();
    memcpy( &data[i], &lockwaitsCount, sizeof( size_t ) );
    i += sizeof( size_t );

    // ends
    std::copy(ti._lockWaits.ends.begin(), ti._lockWaits.ends.end(), (hop::TimeStamp*)&data[i] );
    i += sizeof( hop::TimeStamp ) * lockwaitsCount;

    // deltas
    std::copy( ti._lockWaits.deltas.begin(), ti._lockWaits.deltas.end(), (hop::TimeDuration*)&data[i] );
    i += sizeof( hop::TimeDuration ) * lockwaitsCount;

    // depths
    std::copy( ti._lockWaits.depths.begin(), ti._lockWaits.depths.end(), (hop::TDepth_t*)&data[i] );
    i += sizeof( hop::TDepth_t ) * lockwaitsCount;

    // mutexAddrs
    std::copy( ti._lockWaits.mutexAddrs.begin(), ti._lockWaits.mutexAddrs.end(), (void**)&data[i] );
    i += sizeof( void* ) * lockwaitsCount;
    }

    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, TimelineTrack& ti )
{
    size_t i = 0;

    // Deserializing Traces
    {
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

    // zones
    std::copy((hop::TZoneId_t*)&data[i], ((hop::TZoneId_t*)&data[i]) + tracesCount, std::back_inserter(ti._traces.zones));
    i += sizeof( hop::TZoneId_t ) * tracesCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + tracesCount, std::back_inserter(ti._traces.depths));
    i += sizeof( hop::TDepth_t ) * tracesCount;
    }

    // Deserializing LockWaits
    {
    const size_t lockWaitsCount = *(size_t*)&data[i];
    i += sizeof( size_t );
    // ends
    std::copy((hop::TimeStamp*)&data[i], ((hop::TimeStamp*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.ends));
    i += sizeof( hop::TimeStamp ) * lockWaitsCount;

    // deltas
    std::copy((hop::TimeDuration*)&data[i], ((hop::TimeDuration*)&data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.deltas));
    i += sizeof( hop::TimeDuration ) * lockWaitsCount;

    // depths
    std::copy((hop::TDepth_t*)&data[i], ((hop::TDepth_t*) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.depths));
    i += sizeof( hop::TDepth_t ) * lockWaitsCount;

    // mutexAddrs
    std::copy((void**)&data[i], ((void**) &data[i]) + lockWaitsCount, std::back_inserter(ti._lockWaits.mutexAddrs));
    i += sizeof( void* ) * lockWaitsCount;
    }

    return i;
}

}