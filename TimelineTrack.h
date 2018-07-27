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

class StringDb;

class TimelineTracks
{
  public:
   struct DrawInfo
   {
      float drawPosX, drawPosY, hightlighPct;
      TimeStamp timelineRelativeStartTime;
      TimeStamp timelineAbsoluteStartTime;
      TimeStamp timelineAbsoluteEndTime;
      const StringDb& strDb;
   };

   void update( TimeDuration timelineDuration );
   void draw( const DrawInfo& info );
   void resizeAllTracksToFit();

   TimelineTrack& operator[]( size_t index );
   const TimelineTrack& operator[]( size_t index ) const;
   size_t size() const;
   void resize( size_t size );
   void clear();

  private:
   void drawTraces(
       const TimelineTrack& data,
       uint32_t threadIndex,
       const float posX,
       const float posY,
       const DrawInfo& drawInfo );
   void drawLockWaits( uint32_t threadIndex, const float posX, const float posY );

   std::vector<TimelineTrack> _tracks;
   std::vector< std::pair< size_t, uint32_t > > _highlightedTraces;
   int _lodLevel;
   int _draggedTrack{-1};
};
}

#endif  // TIMELINE_TRACK_H_
