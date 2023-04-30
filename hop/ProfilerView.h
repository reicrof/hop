#ifndef PROFILER_VIEW_H_
#define PROFILER_VIEW_H_

#include "common/Profiler.h"
#include "hop/TimelineTracksView.h"

#include <vector>

namespace hop
{

struct TimelineInfo;
class TimelineMsgArray;

class ProfilerView
{
public:
   ProfilerView( Profiler::SourceType type, int processId, const char* str );
   ProfilerView( std::unique_ptr<NetworkConnection> nc );
   bool fetchClientData();
   void update( TimeDuration timelineDuration, float globalTimeMs );
   bool draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray );

   bool handleHotkey();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void clear();
   void setRecording( bool recording );
   bool recording() const;

   bool saveToFile( const char* path );
   bool openFile( const char* path );

   float canvasHeight() const;
   int lodLevel() const;
   void saveTimelineState( TimeStamp absTime, TimeDuration duration );
   TimeStamp timelineStartTime () const;
   TimeDuration timelineDuration () const;
   const Profiler& data() const;
private:
   Profiler _profiler;
   TimelineTracksView _trackViews;
   // Saved timeline position for this profiler view
   TimeStamp _timelineStart;
   TimeDuration _timelineDuration;
   int _lodLevel;
   float _highlightValue;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
