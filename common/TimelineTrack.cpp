#include "TimelineTrack.h"

#include "Utils.h"

#include <algorithm>
#include <vector>

static void addLockWaitsRecord( hop::LockWaitsRecords* records, size_t index )
{
   if( records->count >= hop::STAMPS_RECORD_SIZE )
   {
      const auto startIt = std::begin(records->indices);
      std::rotate( startIt, startIt + 1, std::end(records->indices) );
      records->count = hop::STAMPS_RECORD_SIZE - 1;
   }

   records->indices[records->count++] = index;
}

static void keepLatestLockWaitsRecords( hop::LockWaitsRecords* records, uint32_t countToKeep )
{
   if( countToKeep < records->count )
   {
      const auto endIt = std::begin(records->indices) + records->count;
      std::rotate( std::begin(records->indices), endIt - countToKeep, endIt );
      records->count = countToKeep;
   }
}

namespace hop
{
void TimelineTrack::setName( hop_str_ptr_t name ) noexcept
{
   _trackName = name;
}

hop_str_ptr_t TimelineTrack::name() const noexcept
{
   return _trackName;
}

void TimelineTrack::addTraces( const TraceData& newTraces )
{
   HOP_ENTER_FUNC( 1 );

   _traces.append( newTraces );

   assert_is_sorted( _traces.entries.ends.begin(), _traces.entries.ends.end() );
   HOP_LEAVE();
}

void TimelineTrack::addLockWaits( const LockWaitData& lockWaits )
{
   HOP_ENTER_FUNC( 2 );
   const size_t prevSize = _lockWaits.mutexAddrs.size();
   _lockWaits.append( lockWaits );

   for( size_t i = 0; i < lockWaits.mutexAddrs.size(); ++i )
   {
      addLockWaitsRecord( &_lockWaitsPerMutex[ lockWaits.mutexAddrs[i] ], prevSize + i );
   }
   HOP_LEAVE();
}

void TimelineTrack::addUnlockEvents( const std::vector<hop_unlock_event_t>& unlockEvents )
{
   // If we did not get any lock events prior to the unlock events, simply ignore them
   if( _lockWaits.entries.ends.empty() ) return;

   HOP_ENTER_FUNC( 3 );
   for( const auto& ue : unlockEvents )
   {
      // Find the list of lockwaits that have not yet been associated with
      // an unlock events for a specific mutex
      const auto lockWaitsIdx = _lockWaitsPerMutex.find( ue.mutexAddress );
      if( lockWaitsIdx != _lockWaitsPerMutex.end() )
      {
         LockWaitsRecords& lwRecords = lockWaitsIdx->second;
         size_t i = 0;
         for (; i < lwRecords.count; ++i)
         {
            if( _lockWaits.entries.ends[lwRecords.indices[i]] < ue.time )
            {
               _lockWaits.lockReleases[lwRecords.indices[i]] = ue.time;
               break;
            }
         }
         // If we found a lockwait that is associted with a specific unlock events,
         // all prior lockwaits can be dismiss as they are either already associated or
         // their unlock event was dropped
         if( i++ != lwRecords.count )
            keepLatestLockWaitsRecords( &lwRecords, lwRecords.count - i );
      }
   }
   HOP_LEAVE();
}

void TimelineTrack::addCoreEvents( const CoreEventData& coreEvents )
{
   HOP_ENTER_FUNC( 4 );
   _coreEvents.append( coreEvents );
   HOP_LEAVE();
}

hop_depth_t TimelineTrack::maxDepth() const noexcept
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
   return serializedSize( ti._traces ) + serializedSize( ti._lockWaits ) +
          serializedSize( ti._coreEvents );
}

size_t serialize( const TimelineTrack& ti, char* data )
{
    const size_t serialSize = serializedSize( ti );
    (void)serialSize; // Removed unused warning
    size_t i = 0;

    i += serialize( ti._traces, &data[i] );
    i += serialize( ti._lockWaits, &data[i] );
    i += serialize( ti._coreEvents, &data[i] );
   
    assert( i == serialSize );

    return i;
}

size_t deserialize( const char* data, TimelineTrack& ti )
{
    size_t i = 0;

    i += deserialize( &data[i], ti._traces );
    i += deserialize( &data[i], ti._lockWaits );
    i += deserialize( &data[i], ti._coreEvents );

    return i;
}

}  // namespace hop