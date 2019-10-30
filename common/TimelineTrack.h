#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"

namespace hop
{

struct TimelineTrack
{
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