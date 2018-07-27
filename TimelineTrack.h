#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"
#include "TraceData.h"

#include <vector>

namespace hop
{

struct TimelineTrack
{
   static float TRACE_HEIGHT;
   static float TRACE_VERTICAL_PADDING;
   static float PADDED_TRACE_SIZE;

   void addTraces( const TraceData& traces );
   void addLockWaits( const LockWaitData& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   TDepth_t maxDepth() const noexcept;
   float maxDisplayedDepth() const noexcept;
   void setTrackHeight( float height );
   bool empty() const;
   TraceData _traces;
   LockWaitData _lockWaits;
   std::vector<UnlockEvent> _unlockEvents;

   // This is the position at which the traces for the current thread ifno
   // are being draw
   float _localTracesVerticalStartPos;
   float _absoluteTracesVerticalStartPos;
   float _trackHeight{9999.0f};

   friend size_t serializedSize( const TimelineTrack& ti );
   friend size_t serialize( const TimelineTrack& ti, char* );
   friend size_t deserialize( const char* data, TimelineTrack& ti );
};
}

#endif  // TIMELINE_TRACK_H_
