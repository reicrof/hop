#ifndef PROFILER_VIEW_H_
#define PROFILER_VIEW_H_

#include "common/Profiler.h"
#include "TimelineTracksView.h"

#include <vector>

namespace hop
{

struct TimelineInfo;
class TimelineMsgArray;

class ProfilerView
{
public:
   ProfilerView( Profiler::SourceType type, int processId, const char* str );
   void fetchClientData();
   void update( float deltaTimeMs, float globalTimeMs, TimeDuration timelineDuration );
   void draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray );

   bool handleHotkey();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void clear();
   void setRecording( bool recording );

   float canvasHeight() const;
   int lodLevel() const;
   const Profiler& data() const;
private:
   Profiler _profiler;
   TimelineTrackViews _trackViews;
   int _lodLevel;
   int _draggedTrack;
   float _highlightValue;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
