#ifndef TIMELINE_H_
#define TIMELINE_H_

#include "Hop.h"
#include "Lod.h"
#include "TraceDetail.h"

#include <vector>

struct ImColor;

namespace hop
{
struct ThreadInfo;
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
   void draw(
       const std::vector<ThreadInfo>& _tracesPerThread,
       const StringDb& strDb  );
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absolutePresentTime() const noexcept;
   void setAbsoluteStartTime( TimeStamp time ) noexcept;
   void setAbsolutePresentTime( TimeStamp time ) noexcept;
   TimeDuration timelineRange() const noexcept;

   const TraceDetails& getTraceDetails() const noexcept;
   void clearTraceDetails();
   void setTraceDetailsDisplayed();

   // Move to first trace
   void moveToStart( AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   // Move to latest time
   void moveToPresentTime( AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   // Move timeline so the specified time is in the middle
   void moveToTime( int64_t timeInMicro, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   void moveToAbsoluteTime( TimeStamp time, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
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

   void pushNavigationState() noexcept;
   void undoNavigation() noexcept;
   void redoNavigation() noexcept;

  private:
   void drawTimeline( const float posX, const float posY );
   void drawTraces( const ThreadInfo& traces, uint32_t threadIndex, const float posX, const float posY, const StringDb& strDb, const ImColor& color );
   void drawLockWaits(const std::vector<ThreadInfo>& infos, uint32_t threadIndex, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );
   void selectTrace( const ThreadInfo& data, uint32_t threadIndex, size_t traceIndex );
   void setStartTime( int64_t timeInMicro, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   void setZoom( TimeDuration microsToDisplay, AnimationType animType = ANIMATION_TYPE_NORMAL );
   void highlightLockOwner(const std::vector<ThreadInfo>& infos, uint32_t threadIndex, const hop::LockWait& highlightedLockWait, const float posX, const float posY );

   int64_t _timelineStart{0};
   TimeDuration _timelineRange{5000000000};
   uint64_t _stepSizeInNanos{1000000};
   TimeStamp _absoluteStartTime{0};
   TimeStamp _absolutePresentTime{0};
   float _rightClickStartPosInCanvas[2] = {};
   float _timelineHoverPos{-1.0f};
   bool _realtime{true};

   std::vector< std::pair< size_t, uint32_t > > _highlightedTraces;

   struct AnimationState
   {
      int64_t targetTimelineStart{0};
      TimeDuration targetTimelineRange{5000000000};
      float highlightPercent{0};
      AnimationType type;
   } _animationState;

   struct Bookmarks
   {
      std::vector< TimeStamp > times;
   } _bookmarks;

   TraceDetails _traceDetails{};

   std::vector< AnimationState > _undoPositionStates, _redoPositionStates;

};
}

#endif  // TIMELINE_H_