#include "TimelineStats.h"

#include "TimelineInfo.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>

static constexpr float STAT_CANVAS_TOP_PADDING = 20.0f;

static bool StatEventCompareTime( const hop::StatEvent& lhs, const hop::StatEvent& rhs)
{
    return lhs.time < rhs.time;
}
static bool StatEventCompareValueInt64( const hop::StatEvent& lhs, const hop::StatEvent& rhs)
{
    return lhs.value.valueInt64 < rhs.value.valueInt64;
}
static bool StatEventCompareValueFloat64( const hop::StatEvent& lhs, const hop::StatEvent& rhs)
{
    return lhs.value.valueFloat64 < rhs.value.valueFloat64;
}

namespace hop
{
    TimelineStats::TimelineStats() : _zoomFactor(1.0f)
    {
        _minMaxValueInt64[0]   = _minMaxValueInt64[2]  = 0;
        _minMaxValueDbl[0]     = _minMaxValueDbl[2]    = 0.0;
        _minMaxValueInt64[1]   = _minMaxValueInt64[3]  = 800;
        _minMaxValueDbl[1]     = _minMaxValueDbl[3]    = 800.0;
    }

   float TimelineStats::canvasHeight() const
   {
      const int64_t minValue = std::min( _minMaxValueInt64[0], (int64_t)std::ceil( _minMaxValueDbl[0] ) );
      const int64_t maxValue = std::max( _minMaxValueInt64[1], (int64_t)std::ceil( _minMaxValueDbl[1] ) );
      return ( llabs( minValue ) + llabs( maxValue ) ) * _zoomFactor;
   }

   void TimelineStats::draw( const TimelineDrawInfo& tinfo, TimelineMsgArray& outMsg )
   {
      const double lastMaxValue = std::max( (double)_minMaxValueInt64[3], _minMaxValueDbl[3] );
      const double maxValue = std::max( (double)_minMaxValueInt64[1], _minMaxValueDbl[1] );
      const double visibleMax = maxValue - tinfo.timeline.scrollAmount + STAT_CANVAS_TOP_PADDING;
      const float relativeZero = tinfo.timeline.canvasPosY + visibleMax;

      ImDrawList* drawList = ImGui::GetWindowDrawList();
      drawList->AddLine(ImVec2(0, relativeZero), ImVec2(3000, relativeZero), 0xFFFFFFFF, 0.4f);

      const auto globalStartTime = tinfo.timeline.globalStartTime;

      // The time range to draw in absolute time
      const TimeStamp firstTraceAbsoluteTime = globalStartTime + tinfo.timeline.relativeStartTime;
      const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + tinfo.timeline.duration;

      StatEvent firstEv = { firstTraceAbsoluteTime, 0, {0} };
      StatEvent lastEv = { lastTraceAbsoluteTime, 0, {0} };
      auto it1 = std::lower_bound( _statEventsInt64.begin(), _statEventsInt64.end(), firstEv, StatEventCompareTime );
      auto it2 = std::upper_bound( _statEventsInt64.begin(), _statEventsInt64.end(), lastEv, StatEventCompareTime );

      if( it1 == it2 ) return; // Nothing to draw here

      const float windowWidthPxl = ImGui::GetWindowWidth();
      for( ; it1 != it2; ++it1 )
      {
         const TimeStamp localTime = it1->time - globalStartTime;
         const auto posPxlX = cyclesToPxl<float>(
            windowWidthPxl,
            tinfo.timeline.duration,
            localTime - tinfo.timeline.relativeStartTime );
         drawList->AddCircle( ImVec2(posPxlX, relativeZero - it1->value.valueInt64), 5.0f, 0XFF0000FF, 5 );
      }

      printf("%f\n", maxValue);
      outMsg.addMoveVerticalPositionMsg( tinfo.timeline.scrollAmount + (maxValue - lastMaxValue) );

       // Save last min/max
       _minMaxValueInt64[2] = _minMaxValueInt64[0];
       _minMaxValueInt64[3] = _minMaxValueInt64[1];
       _minMaxValueDbl[2] = _minMaxValueDbl[0];
       _minMaxValueDbl[3] = _minMaxValueDbl[1];
   }

   void TimelineStats::addStatEventsInt64( const std::vector<StatEvent>& statEvents )
   {
       // Update the min max values
       const auto minMax = std::minmax_element( statEvents.begin(), statEvents.end(), StatEventCompareValueInt64 );
      _minMaxValueInt64[0] = std::min( _minMaxValueInt64[0], minMax.first->value.valueInt64 );
      _minMaxValueInt64[1] = std::max( _minMaxValueInt64[1], minMax.second->value.valueInt64 );

      // Copy the data
      _statEventsInt64.insert( _statEventsInt64.end(), statEvents.begin(), statEvents.end() );
   }

   void TimelineStats::addStatEventsFloat64( const std::vector<StatEvent>& statEvents )
   {


       // Update the min max values
       const auto minMax = std::minmax_element( statEvents.begin(), statEvents.end(), StatEventCompareValueFloat64 );
      _minMaxValueDbl[0] = std::min( _minMaxValueDbl[0], minMax.first->value.valueFloat64 );
      _minMaxValueDbl[1] = std::max( _minMaxValueDbl[1], minMax.second->value.valueFloat64 );

      // Copy the data
      _statEventsFloat64.insert( _statEventsFloat64.end(), statEvents.begin(), statEvents.end() );
   }
}