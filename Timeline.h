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
   void moveToStart( bool animate = true ) noexcept;
   // Move to latest time
   void moveToPresentTime( bool animate = true ) noexcept;
   // Move timeline so the specified time is in the middle
   void moveToTime( int64_t timeInMicro, bool animate = true ) noexcept;
   void moveToAbsoluteTime( TimeStamp time, bool animate ) noexcept;
   // Frame the timeline to display the specified range of time
   void frameToTime( int64_t time, TimeDuration duration ) noexcept;
   void frameToAbsoluteTime( TimeStamp time, TimeDuration duration ) noexcept;
   // Update timeline to always display last race
   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

  private:
   void drawTimeline( const float posX, const float posY );
   void drawTraces( const ThreadInfo& traces, int threadIndex, const float posX, const float posY, const StringDb& strDb, const ImColor& color );
   void drawLockWaits(const std::vector<ThreadInfo>& infos, size_t threadIndex, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );
   void selectTrace( const ThreadInfo& data, uint32_t threadIndex, size_t traceIndex );
   void setStartTime( int64_t timeInMicro, bool withAnimation = true ) noexcept;
   void setZoom( TimeDuration microsToDisplay, bool withAnimation = true );
   void highlightLockOwner(const std::vector<ThreadInfo>& infos, size_t threadIndex, const hop::LockWait& highlightedLockWait, const float posX, const float posY );

   int64_t _timelineStart{0};
   TimeDuration _timelineRange{50000000};
   uint64_t _stepSizeInNanos{1000000};
   TimeStamp _absoluteStartTime{0};
   TimeStamp _absolutePresentTime{0};
   float _rightClickStartPosInCanvas[2] = {};
   float _timelineHoverPos{-1.0f};
   TDepth_t _maxTraceDepthPerThread[HOP_MAX_THREAD_NB] = {};
   bool _realtime{true};

   struct Selection
   {
      static constexpr size_t NONE = -1;
      int threadIndex;
      size_t id{NONE};
      size_t lodIds[ LOD_COUNT ];
   } _selection;

   struct AnimationState
   {
      int64_t targetTimelineStart{0};
      TimeDuration targetTimelineRange{50000000};
   } _animationState;

   TraceDetails _traceDetails{};
};
}

#endif  // TIMELINE_H_