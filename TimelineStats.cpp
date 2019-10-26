#include "TimelineStats.h"

#include "TimelineInfo.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>

static constexpr float STAT_CANVAS_HALF_SIZE   = 5000000.0f;

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

static float valueToPxlPosition( const hop::TimelineInfo& tinfo, double value )
{
    return STAT_CANVAS_HALF_SIZE + tinfo.canvasPosY - value;
}

static void drawValueGrid( ImDrawList* drawList, const hop::TimelineInfo& tinfo, float windowHeight, int64_t gridSize, unsigned color, float size )
{
   const int64_t maxVisibleValue = std::ceil(STAT_CANVAS_HALF_SIZE - tinfo.scrollAmount);
   const int64_t minVisibleValue = std::floor(maxVisibleValue - windowHeight);
   const int64_t maxMult = maxVisibleValue - (maxVisibleValue % gridSize);

   for( int64_t i = maxMult; i > minVisibleValue; i -= gridSize )
   {
      const float linePos = valueToPxlPosition(tinfo, i);
      drawList->AddLine(ImVec2(0, linePos), ImVec2(3000, linePos), color, size);
   }
}

static float drawOrigin( ImDrawList* drawList, const hop::TimelineInfo& tinfo )
{
   const float zeroPxlPos = valueToPxlPosition(tinfo, 0);
   drawList->AddLine(ImVec2(0, zeroPxlPos), ImVec2(3000, zeroPxlPos), 0xFFFFFFFF, 0.4f);
   return zeroPxlPos;
}

namespace hop
{
    TimelineStats::TimelineStats() : _zoomFactor(1.0f)
    {
       _minMaxValueInt64[0] = _minMaxValueInt64[1] = 0;
       _minMaxValueDbl[0]   = _minMaxValueDbl[1]   = 0.0;
    }

   float TimelineStats::canvasHeight() const
   {
      return STAT_CANVAS_HALF_SIZE * 2.0f;
   }

   void TimelineStats::draw( const TimelineDrawInfo& tinfo, TimelineMsgArray& outMsg )
   {
      const float windowHeight = ImGui::GetWindowHeight();
      ImDrawList* drawList = ImGui::GetWindowDrawList();

      drawValueGrid( drawList, tinfo.timeline, windowHeight, 50, 0xFF999999, 0.2f );
      drawValueGrid( drawList, tinfo.timeline, windowHeight, 500, 0xFFCCCCCC, 0.4f );
      const float originPxlPos = drawOrigin( drawList, tinfo.timeline );

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
         drawList->AddCircle( ImVec2(posPxlX, originPxlPos - it1->value.valueInt64), 5.0f, 0XFF0000FF, 5 );
      }
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

   float TimelineStats::originScrollAmount( float windowHeight )
   {
      return STAT_CANVAS_HALF_SIZE - windowHeight * 0.33f;
   }
}