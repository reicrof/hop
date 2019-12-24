#ifndef TIMELINE_TRACKS_VIEW_H_
#define TIMELINE_TRACKS_VIEW_H_

#include <vector>
#include "Lod.h"

namespace hop
{
class Profiler;
class TimelineMsgArray;
struct TimelineInfo;
struct ContextMenu;

struct ContextMenu
{
   size_t traceId{0};
   uint32_t threadIndex{0};
   bool traceClicked{false};
   bool open{false};
};

struct TrackHighlightInfo
{
   float posPxl[2];
   float lengthPxl;
   unsigned color;
};

struct TrackDrawInfo
{
   float absoluteDrawPos[2]; // The absolute position ignores the scroll but not the relative
   float trackHeight{9999.0f};
   LodsData lodsData;
   std::vector< TrackHighlightInfo > highlightInfo;
};

struct TimelineTrackDrawInfo
{
   std::vector<TrackDrawInfo>& drawInfos;
   ContextMenu& contextMenu;
   int& draggedTrack;
   const Profiler& profiler;
   const TimelineInfo& timeline;
   const float paddedTraceHeight;
   const int lodLevel;
   const float highlightValue;
};

void drawTimelineTracks( TimelineTrackDrawInfo& tdi, TimelineMsgArray* msgArray );

} // namespace hop

#endif  // TIMELINE_TRACKS_VIEW_H_