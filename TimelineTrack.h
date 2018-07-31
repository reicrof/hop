#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"
#include "TraceData.h"
#include "TraceSearch.h"
#include "TimelineMessage.h"

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
      float canvasPosX, canvasPosY;
      TimeStamp globalTimelineStartTime;
      TimeStamp relativeTimelineStartTime;
      TimeStamp timelineDuration;
      const StringDb& strDb;
   };

   bool handleHotkey();
   void update( float deltaTimeMs, TimeDuration timelineDuration );
   std::vector< TimelineMessage > draw( const DrawInfo& info );
   void clear();
   float totalHeight() const;
   void resizeAllTracksToFit();
   

   // Vector overloads
   TimelineTrack& operator[]( size_t index );
   const TimelineTrack& operator[]( size_t index ) const;
   size_t size() const;
   void resize( size_t size );

  private:
   void drawTraces(
       const TimelineTrack& data,
       uint32_t threadIndex,
       const float posX,
       const float posY,
       const DrawInfo& drawInfo,
       std::vector< TimelineMessage >& timelineMsg );
   void drawLockWaits(
       uint32_t threadIndex,
       const float posX,
       const float posY,
       std::vector<TimelineMessage>& timelineMsg );
   void drawSearchWindow( const DrawInfo& di, std::vector< TimelineMessage >& timelineMsg );

   std::vector<TimelineTrack> _tracks;
   std::vector< std::pair< size_t, uint32_t > > _highlightedTraces;
   int _lodLevel;
   int _draggedTrack{-1};
   float _highlightValue{0.0f};

   SearchResult _searchRes;
   bool _focusSearchWindow{ false };
   bool _searchWindowOpen{ false };
};
}

#endif  // TIMELINE_TRACK_H_
