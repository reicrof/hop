#ifndef TIMELINE_H_
#define TIMELINE_H_

#include "Hop.h"

#include <vector>

namespace hop
{
struct ThreadInfo;
class Timeline
{
  public:
   void draw(
       const std::vector<ThreadInfo>& _tracesPerThread,
       const std::vector<uint32_t>& threadIds );
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absolutePresentTime() const noexcept;
   void setAbsoluteStartTime( TimeStamp time ) noexcept;
   void setAbsolutePresentTime( TimeStamp time ) noexcept;
   int64_t microsToDisplay() const noexcept;
   float windowWidthPxl() const noexcept;

   void moveToStart() noexcept;
   void moveToPresentTime() noexcept;
   void moveToTime( int64_t timeInMicro ) noexcept;

   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

  private:
   void drawTimeline( const float posX, const float posY );
   void drawTraces( const ThreadInfo& traces, int threadIndex, const float posX, const float posY );
   void drawLockWaits( const ThreadInfo& traces, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );

   int64_t _startMicros{0};
   uint64_t _microsToDisplay{50000};
   int64_t _stepSizeInMicros{1000};
   TimeStamp _absoluteStartTime{};
   TimeStamp _absolutePresentTime{};
   float _rightClickStartPosInCanvas[2] = {};
   TDepth_t _maxTraceDepthPerThread[MAX_THREAD_NB] = {};
   float _windowWidthPxl{0};
   bool _realtime{true};
};
}

#endif  // TIMELINE_H_