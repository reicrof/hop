#ifndef TIMELINE_H_
#define TIMELINE_H_

#include "Hop.h"
#include "Lod.h"
#include "TraceDetail.h"

#include <vector>

struct ImColor;

namespace hop
{
struct TimelineTrack;
class StringDb;
class Timeline
{
  public:
   enum AnimationType
   {
      ANIMATION_TYPE_NONE,
      ANIMATION_TYPE_NORMAL,
      ANIMATION_TYPE_FAST
   };

   void update( float deltaTimeMs ) noexcept;
   void draw( std::vector<TimelineTrack>& tracks, const StringDb& strDb  );
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absolutePresentTime() const noexcept;
   void setAbsoluteStartTime( TimeStamp time ) noexcept;
   void setAbsolutePresentTime( TimeStamp time ) noexcept;

   TimeStamp timelineStart() const noexcept;
   TimeStamp absoluteTimelineStart() const noexcept;
   TimeStamp absoluteTimelineEnd();
   TimeDuration timelineRange() const noexcept;
   float verticalPosPxl() const noexcept;
   float maxVerticalPosPxl() const noexcept;
   int currentLodLevel() const noexcept;

   TraceDetails& getTraceDetails() noexcept;
   void clearTraceDetails();
   void setTraceDetailsDisplayed();

   void clearTraceStats();

   // Move to first trace
   void moveToStart( AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   // Move to latest time
   void moveToPresentTime( AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   // Move timeline so the specified time is in the middle
   void moveToTime( int64_t timeInMicro, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   void moveToAbsoluteTime( TimeStamp time, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   // Move timeline vertically to specified pixel position
   void moveVerticalPositionPxl( float positionPxl, AnimationType animType = ANIMATION_TYPE_NORMAL );
   // Frame the timeline to display the specified range of time
   void frameToTime( int64_t time, TimeDuration duration ) noexcept;
   void frameToAbsoluteTime( TimeStamp time, TimeDuration duration ) noexcept;
   // Update timeline to always display last race
   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

   void addTraceToHighlight( const std::pair< size_t, uint32_t >& trace );
   void clearHighlightedTraces();

   void nextBookmark() noexcept;
   void previousBookmark() noexcept;
   void clearBookmarks();

   void pushNavigationState() noexcept;
   void undoNavigation() noexcept;
   void redoNavigation() noexcept;

   friend size_t serializedSize( const Timeline& timeline );
   friend size_t serialize( const Timeline& timeline, char* data );
   friend size_t deserialize( const char* data, Timeline& timeline );

  private:
   struct AnimationState
   {
      int64_t targetTimelineStart{0};
      TimeDuration targetTimelineRange{5000000000};
      float targetVerticalPosPxl{0.0f};
      float highlightPercent{0.0f};
      AnimationType type;
   } _animationState;

   struct Bookmarks
   {
      std::vector< TimeStamp > times;
   } _bookmarks;

   struct LockOwnerInfo
   {
      LockOwnerInfo( TimeDuration dur, uint32_t tIdx ) : lockDuration(dur), threadIndex(tIdx){}
      TimeDuration lockDuration{0};
      uint32_t threadIndex{0};
   };

   struct ContextMenu
   {
      size_t traceId{0};
      uint32_t threadIndex{0};
      bool open{false};
   } _contextMenuInfo;

   void drawTimeline( const float posX, const float posY );
   void drawTraces( const TimelineTrack& traces, uint32_t threadIndex, const float posX, const float posY, const StringDb& strDb );
   void drawLockWaits(const std::vector<TimelineTrack>& infos, uint32_t threadIndex, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY, std::vector<TimelineTrack>& tracesPerThread );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );
   void setStartTime( int64_t timeInMicro, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   void setZoom( TimeDuration microsToDisplay, AnimationType animType = ANIMATION_TYPE_NORMAL );
   std::vector< LockOwnerInfo > highlightLockOwner(const std::vector<TimelineTrack>& infos, uint32_t threadIndex, uint32_t hoveredLwIndex, const float posX, const float posY );

   int64_t _timelineStart{0};
   TimeDuration _timelineRange{5000000000};
   uint64_t _stepSizeInNanos{1000000};
   TimeStamp _absoluteStartTime{0};
   TimeStamp _absolutePresentTime{0};
   float _verticalPosPxl{0.0f};
   float _rightClickStartPosInCanvas[2] = {};
   float _ctrlRightClickStartPosInCanvas[2] = {};
   float _timelineHoverPos{-1.0f};
   int _lodLevel;
   bool _realtime{true};
   int _draggedTrack{-1};

   std::vector< std::pair< size_t, uint32_t > > _highlightedTraces;

   TraceDetails _traceDetails{};
   TraceStats _traceStats{ 0, 0, 0, 0, 0, std::vector< float >(), false, false };

   std::vector< AnimationState > _undoPositionStates, _redoPositionStates;

};
}

#endif  // TIMELINE_H_