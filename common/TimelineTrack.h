#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"
#include "TraceData.h"

#include <unordered_map>

namespace hop
{

constexpr uint32_t STAMPS_RECORD_SIZE = 16;
struct LockWaitsRecords
{
   size_t indices[STAMPS_RECORD_SIZE];
   uint32_t count;
};

struct TimelineTrack
{
   void setName( hop_str_ptr_t name ) noexcept;
   hop_str_ptr_t name() const noexcept;
   void addTraces( const TraceData& traces );
   void addLockWaits( const LockWaitData& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   void addCoreEvents( const CoreEventData& coreEvents );
   hop_depth_t maxDepth() const noexcept;
   bool empty() const;

   TraceData _traces;
   LockWaitData _lockWaits;
   CoreEventData _coreEvents;
   hop_str_ptr_t _trackName{0};

   std::unordered_map< void*, LockWaitsRecords > _lockWaitsPerMutex;
};

size_t serializedSize( const TimelineTrack& ti );
size_t serialize( const TimelineTrack& ti, char* );
size_t deserialize( const char* data, TimelineTrack& ti );

} //  namespace hop

#endif // TIMELINE_TRACK_H_