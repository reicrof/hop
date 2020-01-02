#ifndef TIMELINE_TRACKS_VIEW_H_
#define TIMELINE_TRACKS_VIEW_H_

#include <vector>
#include "Lod.h"
#include "SearchWindow.h"
#include "TraceStats.h"

struct HighlightInfo;

namespace hop
{
class Profiler;
class TimelineMsgArray;
struct TimelineInfo;

// External data coming from the profiler and the timeline
struct TimelineTrackDrawData
{
   const Profiler& profiler;
   const TimelineInfo& timeline;
   const int lodLevel;
   const float highlightValue;
};

class TimelineTracksView
{
public:
   uint32_t count() const;
   bool hidden( uint32_t trackIdx ) const;
   float trackHeightWithThreadLabel( uint32_t trackIdx ) const;
   float trackAbsoluteDrawPosY( uint32_t trackIdx ) const;

   void update( const Profiler& profiler );
   void draw( const TimelineTrackDrawData& data, TimelineMsgArray* msgArray );

   // Returns true if the mouse/keys was handled by the tracks
   bool handleHotkeys();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void setTrackHeight( uint32_t trackIdx, float height );
   void clear();

   // Per track view data
   struct TrackViewData
   {
      LodsData traceLodsData;
      LodsData lockwaitsLodsData;
      float absoluteDrawPos[2];
      float relativePosY; // The absolute position ignores the scroll but not the relative
      float trackHeight{9999.0f};
      Depth_t maxDepth;
   };
   struct ContextMenu
   {
      size_t traceId{0};
      uint32_t threadIndex{0};
      bool traceClicked{false};
      bool open{false};
   };

private:
   void drawSearchWindow(
      const hop::TimelineTrackDrawData& data,
      std::vector<HighlightInfo>& traceToHighlight,
      hop::TimelineMsgArray* msgArray );
   void drawTraceDetailsWindow(
      const hop::TimelineTrackDrawData& data,
      std::vector<HighlightInfo>& traceToHighlight,
      hop::TimelineMsgArray* msgArray );
   void drawContextMenu( const TimelineTrackDrawData& data );
   void resizeAllTracksToFit();
   void setAllTracksCollapsed( bool collapsed );

   std::vector<TrackViewData> _tracks;
   ContextMenu _contextMenu;
   SearchResult _searchResult;
   TraceDetails _traceDetails;
   TraceStats _traceStats;
   int _draggedTrack{-1};
};

} // namespace hop

#endif  // TIMELINE_TRACKS_VIEW_H_