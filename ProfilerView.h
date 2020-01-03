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
   void update( float globalTimeMs, TimeDuration timelineDuration );
   void draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray );

   bool handleHotkey();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void clear();
   void setRecording( bool recording );

   bool saveToFile( const char* path );
   bool openFile( const char* path );

   float canvasHeight() const;
   int lodLevel() const;
   const Profiler& data() const;
private:
   Profiler _profiler;
   TimelineTracksView _trackViews;
   int _lodLevel;
   float _highlightValue;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
