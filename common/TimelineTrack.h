#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"
#include "TraceData.h"

namespace hop
{

struct TimelineTrack
{
   void setName( StrPtr_t name ) noexcept;
   StrPtr_t name() const noexcept;
   void addTraces( const TraceData& traces );
   void addLockWaits( const LockWaitData& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   void addCoreEvents( const std::vector<CoreEvent>& coreEvents );
   Depth_t maxDepth() const noexcept;
   bool empty() const;

   TraceData _traces;
   LockWaitData _lockWaits;
   CoreEventData _coreEvents;
   StrPtr_t _trackName{0};
};

size_t serializedSize( const TimelineTrack& ti );
size_t serialize( const TimelineTrack& ti, char* );
size_t deserialize( const char* data, TimelineTrack& ti );

} //  namespace hop

#endif // TIMELINE_TRACK_H_