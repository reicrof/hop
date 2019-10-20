#include "TimelineStats.h"

#include "TimelineInfo.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>

namespace hop
{
    TimelineStats::TimelineStats() : _zoomFactor( 1.0f ), _minRange( -500 ), _maxRange( 500 )
    {

    }

   float TimelineStats::canvasHeight() const
   {
      return ( fabs( _minRange ) + fabs( _maxRange ) ) * _zoomFactor;
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

      StatEvent firstEv = { firstTraceAbsoluteTime, 0, {0} };
      StatEvent lastEv = { lastTraceAbsoluteTime, 0, {0} };
      auto cmp = []( const StatEvent& lhs, const StatEvent& rhs) { return lhs.time < rhs.time; };
      auto it1 = std::lower_bound( _statEventsInt64.begin(), _statEventsInt64.end(), firstEv, cmp );
      auto it2 = std::upper_bound( _statEventsInt64.begin(), _statEventsInt64.end(), lastEv, cmp );

      if( it1 == it2 ) return; // Nothing to draw here

      const float windowWidthPxl = ImGui::GetWindowWidth();
      for( ; it1 != it2; ++it1 )
      {
         const TimeStamp localTime = it1->time - globalStartTime;
         const auto posPxlX = cyclesToPxl<float>(
            windowWidthPxl,
            tinfo.timeline.duration,
            localTime - tinfo.timeline.relativeStartTime );
         drawList->AddCircle( ImVec2(posPxlX, relativeZero - it1->value.valueInt64), 5.0f, 0XFF0000FF );
      }
   }

   void TimelineStats::addStatEventsInt64( const std::vector<StatEvent>& statEvents )
   {
      _statEventsInt64.insert( _statEventsInt64.end(), statEvents.begin(), statEvents.end() );
   }

   void TimelineStats::addStatEventsFloat64( const std::vector<StatEvent>& statEvents )
   {
      _statEventsFloat64.insert( _statEventsFloat64.end(), statEvents.begin(), statEvents.end() );
   }
}