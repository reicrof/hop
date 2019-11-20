#ifndef TIMELINE_TRACKS_VIEW_H_
#define TIMELINE_TRACKS_VIEW_H_

#include <vector>

namespace hop
{
class Profiler;
class TimelineMsgArray;
struct TimelineInfo;

struct TrackDrawInfo
{
   float localDrawPos[2];    // This is the position at which the track is drawn in canvas coord
   float absoluteDrawPos[2]; // The absolute position ignores the scroll but not the relative
   float trackHeight{9999.0f};
};

struct TimelineTrackDrawInfo
{
   std::vector< TrackDrawInfo >& drawInfos;
   const Profiler& profiler;
   const TimelineInfo& timeline;
   float paddedTraceHeight;
};

void drawTimelineTracks( TimelineTrackDrawInfo& tdi, TimelineMsgArray* msgArray );

} // namespace hop

#endif  // TIMELINE_TRACKS_VIEW_H_