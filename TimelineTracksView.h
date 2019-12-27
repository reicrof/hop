#ifndef TIMELINE_TRACKS_VIEW_H_
#define TIMELINE_TRACKS_VIEW_H_

#include <vector>
#include "Lod.h"
#include "SearchWindow.h"

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

struct TraceHighlight
{
   float posPxl[2];
   float lengthPxl;
   unsigned color;
};

struct TrackViewData
{
   LodsData lodsData;
   std::vector< TraceHighlight > highlightInfo;
   float absoluteDrawPos[2]; // The absolute position ignores the scroll but not the relative
   float trackHeight{9999.0f};
   Depth_t maxDepth;
};

struct TimelineTrackViews
{
   std::vector<TrackViewData> tracks;
   ContextMenu contextMenu;
   SearchResult searchResult;
   int draggedTrack{-1};
};

// External data coming from the profiler and the timeline
struct TimelineTrackDrawData
{
   const Profiler& profiler;
   const TimelineInfo& timeline;
   const int lodLevel;
   const float highlightValue;
};

void updateTimelineTracks( TimelineTrackViews& tracksView, const hop::Profiler& profiler );

void drawTimelineTracks( TimelineTrackViews& tracksView, const TimelineTrackDrawData& data, TimelineMsgArray* msgArray );

// Returns true if a hotkey was handled by the timeline tracks
bool handleTimelineTracksHotKey( TimelineTrackViews& tracksView );

} // namespace hop

#endif  // TIMELINE_TRACKS_VIEW_H_