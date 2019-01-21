#ifndef TIMELINE_TRACK_H_
#define TIMELINE_TRACK_H_

#include "Hop.h"
#include "TraceData.h"
#include "TraceSearch.h"
#include "TraceDetail.h"
#include "TimelineInfo.h"

#include <tuple>
#include <vector>

namespace hop
{

struct TimelineTrack
{
   static float TRACE_HEIGHT;
   static float TRACE_VERTICAL_PADDING;
   static float PADDED_TRACE_SIZE;

   void setTrackName( StrPtr_t name ) noexcept;
   StrPtr_t trackName() const noexcept;
   void addTraces( const TraceData& traces );
   void addLockWaits( const LockWaitData& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   void addCoreEvents( const std::vector<CoreEvent>& coreEvents );
   Depth_t maxDepth() const noexcept;
   bool hidden() const noexcept;
   float height() const noexcept;
   float heightWithThreadLabel() const noexcept;
   void setTrackHeight( float height );
   bool empty() const;
   TraceData _traces;
   LockWaitData _lockWaits;
   CoreEventData _coreEvents;

   struct HighlightDrawInfo
   {
      float posPxlX, posPxlY, lengthPxl;
      uint32_t color;
   };
   std::vector< HighlightDrawInfo > _highlightsDrawData;

   // This is the position at which the track is drawn in canvas coord
   // The absolute position ignores the scroll but not the relative
   float _localDrawPos[2];
   float _absoluteDrawPos[2];

private:
   float _trackHeight{9999.0f};
   StrPtr_t _trackName{0};

   friend size_t serializedSize( const TimelineTrack& ti );
   friend size_t serialize( const TimelineTrack& ti, char* );
   friend size_t deserialize( const char* data, TimelineTrack& ti );
};

class StringDb;

struct LockOwnerInfo
{
   LockOwnerInfo( TimeDuration dur, uint32_t tIdx ) : lockDuration(dur), threadIndex(tIdx){}
   TimeDuration lockDuration{0};
   uint32_t threadIndex{0};
};

struct TimelineTracksDrawInfo
{
   const TimelineInfo& timeline;
   const StringDb& strDb;
};

class TimelineTracks
{
  public:
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );
   bool handleHotkey();
   void update( float deltaTimeMs, TimeDuration timelineDuration );
   std::vector< TimelineMessage > draw( const TimelineTracksDrawInfo& info );
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
       const uint32_t threadIndex,
       const float posX,
       const float posY,
       const TimelineTracksDrawInfo& drawInfo,
       std::vector< TimelineMessage >& timelineMsg );
   void drawLockWaits(
       const uint32_t threadIndex,
       const float posX,
       const float posY,
       const TimelineTracksDrawInfo& drawInfo,
       std::vector<TimelineMessage>& timelineMsg );
   void drawSearchWindow( const TimelineTracksDrawInfo& di, std::vector< TimelineMessage >& timelineMsg );
   void drawTraceDetailsWindow( const TimelineTracksDrawInfo& di, std::vector< TimelineMessage >& timelineMsg );
   void drawContextMenu( const TimelineTracksDrawInfo& info );
   std::vector<LockOwnerInfo> highlightLockOwner(
       const uint32_t threadIndex,
       const uint32_t hoveredLwIndex,
       const TimelineTracksDrawInfo& info );
   void addTraceToHighlight( size_t traceId, uint32_t threadIndex, const TimelineTracksDrawInfo& drawInfo );

   std::vector<TimelineTrack> _tracks;
   int _lodLevel;
   int _draggedTrack{-1};
   float _highlightValue{0.0f};

   SearchResult _searchRes;

   TraceDetails _traceDetails{};

   TraceStats _traceStats{ 0, 0, 0, 0, 0, std::vector< float >(), false, false };

   struct ContextMenu
   {
      size_t traceId{0};
      uint32_t threadIndex{0};
      bool traceClick{false};
      bool open{false};
   } _contextMenuInfo;
};
}

#endif  // TIMELINE_TRACK_H_
