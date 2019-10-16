#include "TimelineStats.h"

#include "TimelineInfo.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>

static double valueAsDbl( hop::StatEvent ev )
{
    switch( ev.valueType )
    {
    case hop::STAT_EVENT_INT64:
        return ev.value.int64_;
    case hop::STAT_EVENT_UINT64:
        return ev.value.uint64_;
    case hop::STAT_EVENT_FLOAT:
        return ev.value.float_;
    }
    return 0.0;
}

namespace hop
{
    TimelineStats::TimelineStats() : _zoomFactor( 1.0f ), _minRange( -500 ), _maxRange( 500 )
    {

    }

   float TimelineStats::canvasHeight() const
   {
      return ( std::abs( _minRange ) + std::abs( _maxRange ) ) * _zoomFactor;
   }

   void TimelineStats::draw( const TimelineDrawInfo& tinfo, TimelineMsgArray& /*outMsg*/ )
   {
      const double visibleMax = _maxRange - tinfo.timeline.scrollAmount;
      const float relativeZero = tinfo.timeline.canvasPosY + visibleMax;

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      drawList->AddLine(ImVec2(0, relativeZero), ImVec2(3000, relativeZero), 0xFFFFFFFF, 0.4f);

      const auto globalStartTime = tinfo.timeline.globalStartTime;

      // The time range to draw in absolute time
      const TimeStamp firstTraceAbsoluteTime = globalStartTime + tinfo.timeline.relativeStartTime;
      const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + tinfo.timeline.duration;

      StatEvent firstEv = { firstTraceAbsoluteTime, 0, 0, STAT_EVENT_INT64 };
      StatEvent lastEv = { lastTraceAbsoluteTime, 0, 0, STAT_EVENT_INT64 };
      auto cmp = []( const StatEvent& lhs, const StatEvent& rhs) { return lhs.time < rhs.time; };
      auto it1 = std::lower_bound( _statEvents.begin(), _statEvents.end(), firstEv, cmp );
      auto it2 = std::upper_bound( _statEvents.begin(), _statEvents.end(), lastEv, cmp );

      if( it1 == it2 ) return; // Nothing to draw here

      const float windowWidthPxl = ImGui::GetWindowWidth();
      for( ; it1 != it2; ++it1 )
      {
         const TimeStamp localTime = it1->time - globalStartTime;
         const auto posPxlX = cyclesToPxl<float>(
            windowWidthPxl,
            tinfo.timeline.duration,
            localTime - tinfo.timeline.relativeStartTime );
         double value = valueAsDbl( *it1 );
         drawList->AddCircle( ImVec2(posPxlX, relativeZero - value), 5.0f, 0XFF0000FF );
      }
   }

   void TimelineStats::addStatEvents( const std::vector<StatEvent>& statEvents )
   {
      _statEvents.insert( _statEvents.end(), statEvents.begin(), statEvents.end() );
   }
}