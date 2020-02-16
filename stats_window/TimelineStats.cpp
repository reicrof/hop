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

static double updateStepSize( double curStep, float zoomFactor )
{
   const double minStepSize = 20.0;
   const double maxStepSize = 100.0;
   const double stepSize = curStep / zoomFactor;
   if (stepSize < minStepSize)
   {
      return std::max(curStep * 5.0, 10.0);
   }
   else if (stepSize > maxStepSize)
   {
      return curStep > 9.0 ? curStep / 5.0 : 1.0;
   }

   return curStep;
}

static float valueToPxlPosition( const hop::TimelineInfo& tinfo, float zoomFactor, double value )
{
    return STAT_CANVAS_HALF_SIZE + tinfo.canvasPosY - (value / zoomFactor);
}

static void drawValueGrid( ImDrawList* drawList, const hop::TimelineInfo& tinfo, float windowHeight, float zoomFactor, float gridSize, unsigned lineColor, float lineSize )
{
   const float maxVisibleValue = (STAT_CANVAS_HALF_SIZE - tinfo.scrollAmount) * zoomFactor;
   const float minVisibleValue = (STAT_CANVAS_HALF_SIZE - tinfo.scrollAmount - windowHeight) * zoomFactor;
   const float maxMult = maxVisibleValue - (fmodf(maxVisibleValue, gridSize));

   static const auto TEXT_SIZE = ImGui::CalcTextSize( "-1" );

   char valueTxt[32];
   for( int64_t i = maxMult; i > minVisibleValue; i -= gridSize )
   {
      const float linePos = valueToPxlPosition(tinfo, zoomFactor, i);
      drawList->AddLine(ImVec2(0, linePos), ImVec2(3000, linePos), lineColor, lineSize);
      snprintf( valueTxt, sizeof( valueTxt ), "%d", (int)i );
      drawList->AddText(ImVec2(5, linePos - TEXT_SIZE.y ), lineColor, valueTxt );
   }
}

static float drawOrigin( ImDrawList* drawList, const hop::TimelineInfo& tinfo, float zoomFactor )
{
   const float zeroPxlPos = valueToPxlPosition(tinfo, zoomFactor, 0);
   drawList->AddLine(ImVec2(0, zeroPxlPos), ImVec2(3000, zeroPxlPos), 0xFFFFFFFF, 0.4f);
   return zeroPxlPos;
}

namespace hop
{
    TimelineStats::TimelineStats() : _zoomFactor(1.0f), _valueSteps(50.0)
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
      const float halfWndHeight   = windowHeight * 0.5f;
      const float canvasStartPxl = STAT_CANVAS_HALF_SIZE - tinfo.timeline.scrollAmount - halfWndHeight;
      const double canvasStartValue = canvasStartPxl * _zoomFactor;
      if( ImGui::DragFloat( "zoom", &_zoomFactor, 1.0f, 0.01f, 1000000.0f, "%.4f", 10.0f ) )
      {
         const double newTopCanvasPos = canvasStartValue / _zoomFactor;
         const float newScroll = STAT_CANVAS_HALF_SIZE - newTopCanvasPos - halfWndHeight;
         outMsg.addMoveVerticalPositionMsg( newScroll, false );
      }

      const double gridSize       = updateStepSize(_valueSteps, _zoomFactor);
      const double coarseGridSize = gridSize * 10.0;
      _valueSteps = gridSize;

      ImDrawList* drawList = ImGui::GetWindowDrawList();

      drawValueGrid( drawList, tinfo.timeline, windowHeight, _zoomFactor, gridSize, 0XFF888888, 1.0f );
      drawValueGrid( drawList, tinfo.timeline, windowHeight, _zoomFactor, coarseGridSize, 0xFFAAAAAA, 2.0f );
      const float originPxlPos = drawOrigin( drawList, tinfo.timeline, _zoomFactor );

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
         const float posPxlY =  valueToPxlPosition(tinfo.timeline, _zoomFactor, it1->value.valueInt64);
         drawList->AddCircle( ImVec2(posPxlX, posPxlY), 5.0f, 0XFF0000FF, 5 );
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