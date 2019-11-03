#include "TimelineTrack.h"

#include "Utils.h"

#include <vector>

namespace hop
{
void TimelineTrack::setTrackName( StrPtr_t name ) noexcept
{
   _trackName = name;
}

StrPtr_t TimelineTrack::trackName() const noexcept
{
   return _trackName;
}

void TimelineTrack::addTraces( const TraceData& newTraces )
{
   HOP_ZONE( HOP_ZONE_COLOR_4 );
   HOP_PROF_FUNC();

   _traces.append( newTraces );

   assert_is_sorted( _traces.entries.ends.begin(), _traces.entries.ends.end() );
}

void TimelineTrack::addLockWaits( const LockWaitData& lockWaits )
{
   HOP_PROF_FUNC();
   const size_t prevSize = _lockWaits.mutexAddrs.size();
   _lockWaits.append( lockWaits );
}

void TimelineTrack::addUnlockEvents(const std::vector<UnlockEvent>& /*unlockEvents*/)
{
   // If we did not get any lock events prior to the unlock events, simply ignore them
   if( _lockWaits.entries.ends.empty() ) return;

//    HOP_PROF_FUNC();
//    for( const auto& ue : unlockEvents )
//    {
//       // Find the list of lockwaits that have not yet been associated with
//       // an unlock events for a specific mutex
//       const auto lockWaitsIdx = _lockWaitsPerMutex.find( ue.mutexAddress );
//       if(lockWaitsIdx != _lockWaitsPerMutex.end() )
//       {
//          std::vector< TimeStamp >& lockwaitIdx = lockWaitsIdx->second;
//          size_t i = 0;
//          for (; i < lockwaitIdx.size(); ++i)
//          {
//             if( _lockWaits.entries.ends[lockwaitIdx[i]] < ue.time )
//             {
//                _lockWaits.lockReleases[lockwaitIdx[i]] = ue.time;
//                break;
//             }
//          }
//          // If we found a lockwait that is associted with a specific unlock events,
//          // all prior lockwaits can be dismiss and are either already associated or
//          // their unlock event was dropped
//          if( i++ != lockwaitIdx.size() )
//             lockwaitIdx.erase( lockwaitIdx.begin(), lockwaitIdx.begin() + i );
//       }
//    }
}

void TimelineTrack::addCoreEvents( const std::vector<CoreEvent>& coreEvents )
{
   _coreEvents.data.insert( _coreEvents.data.end(), coreEvents.begin(), coreEvents.end() );
   assert_is_sorted( _coreEvents.data.begin(), _coreEvents.data.end() );
}

Depth_t TimelineTrack::maxDepth() const noexcept
{
   return _traces.entries.maxDepth;
}

bool TimelineTrack::empty() const
{
   return _traces.entries.ends.empty();
}

/**
 * Serialization functions
 */

size_t serializedSize( const TimelineTrack& ti )
{
   return serializedSize( ti._traces ) + serializedSize( ti._lockWaits );
}

size_t serialize( const TimelineTrack& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    size_t i = 0;

    i += serialize( ti._traces, &data[i] );
    i += serialize( ti._lockWaits, &data[i] );
   
    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, TimelineTrack& ti )
{
    size_t i = 0;

    i += deserialize( &data[i], ti._traces );
    i += deserialize( &data[i], ti._lockWaits );

    return i;
}

}  // namespace hop