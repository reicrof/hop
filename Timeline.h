#ifndef TIMELINE_H_
#define TIMELINE_H_

#include "Hop.h"
#include "TimelineInfo.h"

#include <vector>

struct ImColor;

namespace hop
{
class Timeline
{
  public:
   enum AnimationType
   {
      ANIMATION_TYPE_NONE,
      ANIMATION_TYPE_NORMAL,
      ANIMATION_TYPE_FAST
   };

   enum DisplayType
   {
      DISPLAY_CYCLES,
      DISPLAY_TIMES
   };

   void update( float deltaTimeMs ) noexcept;
   void draw();
   void clear();
   void beginDrawCanvas( float canvasHeightPxl );
   void endDrawCanvas();
   void drawOverlay();
   TimelineInfo constructTimelineInfo() const noexcept;

   bool handleMouse( float posX, float posY, bool lmPressed, bool rmPressed, float wheel );
   bool handleHotkey();
   void handleDeferredActions( const std::vector< TimelineMessage >& msg );


   /*
                           Visible Timeline
   ---------------------|||||||||||||||||||||||------------ > time
   ^                    ^                     ^            ^
   | global_start_time  |                     |           | global_end_time
                        |                     |
                        |                     | absolute_end_time == relative_end_time + global_start_time
                        |
                        | absolute_start_time == (relative_start_time + global_start_time)
   */

   // Global time represent the global minimum/maximum of the timeline
   // not considering any pan/zoom
   TimeStamp globalStartTime() const noexcept;
   TimeStamp globalEndTime() const noexcept;
   void setGlobalStartTime( TimeStamp time ) noexcept;
   void setGlobalEndTime( TimeStamp time ) noexcept;

   // Absolute timeline start is the current starting time of the timeline
   // considering zoom/pan NOT offsetted by the global time.
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absoluteEndTime() const noexcept;

   // Relative timeline start is the current starting time of the timeline
   // considering zoom/pan offsetted by the global time.
   TimeStamp relativeStartTime() const noexcept;
   TimeStamp relativeEndTime() const noexcept;
   TimeDuration duration() const noexcept;

   float verticalPosPxl() const noexcept;
   float maxVerticalPosPxl() const noexcept;

   float canvasPosX() const noexcept;
   float canvasPosY() const noexcept;
   float canvasPosYWithScroll() const noexcept;

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
   void frameToTime( int64_t time, TimeDuration duration, bool pushNavState ) noexcept;
   void frameToAbsoluteTime( TimeStamp time, TimeDuration duration, bool pushNavState ) noexcept;
   // Update timeline to always display last race
   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

   void setDisplayType( DisplayType type );

   void clearHighlightedTraces();

   void nextBookmark() noexcept;
   void previousBookmark() noexcept;

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
      AnimationType type{ANIMATION_TYPE_NONE};
   } _animationState;

   struct Bookmarks
   {
      std::vector< TimeStamp > times;
   } _bookmarks;

   void drawTimeline( float posX, float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mouseWheel );
   void zoomOn( int64_t cycleToZoomOn, float zoomFactor );
   void setStartTime( int64_t timeInCyle, AnimationType animType = ANIMATION_TYPE_NORMAL ) noexcept;
   void setZoom( TimeDuration microsToDisplay, AnimationType animType = ANIMATION_TYPE_NORMAL );

   // Origin of the timeline in absolute time
   TimeStamp _globalStartTime{0};
   TimeStamp _globalEndTime{0};

   // Current timeline start and range
   int64_t _timelineStart{0};
   uint64_t _duration{5000000000};

   uint64_t _stepSize{1000000};
   bool _realtime{true};

   // Drawing Data
   float _verticalPosPxl{0.0f};
   float _timelineHoverPos{-1.0f};
   float _timelineDrawPosition[2] = {};
   float _canvasDrawPosition[2] = {};
   float _canvasHeight{0.0f};
   DisplayType _displayType{DISPLAY_TIMES};

   int64_t _rangeZoomCycles[2] = {};
   int64_t _rangeSelectTimeStamp[2] = {};

   std::vector< AnimationState > _undoPositionStates, _redoPositionStates;

};
}

#endif  // TIMELINE_H_