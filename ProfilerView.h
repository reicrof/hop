#ifndef PROFILER_VIEW_H_
#define PROFILER_VIEW_H_

#include "common/Profiler.h"

#include <vector>

namespace hop
{

class Profiler;

struct TrackDrawInfo
{
   float localDrawPos[2];    // This is the position at which the track is drawn in canvas coord
   float absoluteDrawPos[2]; // The absolute position ignores the scroll but not the relative
   float trackHeight{9999.0f};
};

class ProfilerView
{
public:
   ProfilerView( Profiler::SourceType type, int processId, const char* str );
   void update( float deltaTimeMs, float globalTimeMs, TimeDuration timelineDuration );
   void draw( float drawPosX, float drawPosY, float windowWidth, float windowHeight );

   void handleHotkey();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void clear();
   void setRecording( bool recording );

   const int lodLevel() const;
   const Profiler& data() const;
private:
   Profiler _profiler;
   std::vector<TrackDrawInfo> _trackDrawInfos;
   int _lodLevel;
   int _draggedTrack;
   float _highlightValue;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
