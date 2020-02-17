#ifndef PROFILER_VIEW_H_
#define PROFILER_VIEW_H_

#include "common/Profiler.h"
#include "hop/TimelineTracksView.h"
#include "hop/TimelineStatsView.h"

#include <vector>

namespace hop
{

struct TimelineInfo;
class TimelineMsgArray;

class ProfilerView
{
public:

   enum class Type
   {
      PROFILER,
      STATS,
      COUNT
   };

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

   Type type() const;
   void setType( Type type );

   float verticalPos() const;
   void setVerticalPos( float pos );

   float canvasHeight() const;
   int lodLevel() const;
   const Profiler& data() const;
private:
   Profiler _profiler;
   TimelineTracksView _trackViews;
   TimelineStatsView _timelineStats;

   Type _type;
   float _viewsVerticalPos[(int)Type::COUNT];
   int _lodLevel;
   float _highlightValue;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
