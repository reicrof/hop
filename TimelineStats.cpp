#include "TimelineStats.h"

#include "TimelineInfo.h"
#include "Utils.h"

#include "imgui/imgui.h"

#include <algorithm>

namespace hop
{
   std::vector< TimelineMessage > TimelineStats::draw( const const TimelineInfo& tinfo, const StringDb& strDb )
   {
      std::vector< TimelineMessage > messages;

      const auto globalStartTime = tinfo.globalStartTime;

      // The time range to draw in absolute time
      const TimeStamp firstTraceAbsoluteTime = globalStartTime + tinfo.relativeStartTime;
      const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + tinfo.duration;

      StatEvent firstEv = { firstTraceAbsoluteTime, 0, 0, STAT_EVENT_INT64 };
      StatEvent lastEv = { lastTraceAbsoluteTime, 0, 0, STAT_EVENT_INT64 };
      auto cmp = []( const StatEvent& lhs, const StatEvent& rhs) { return lhs.time < rhs.time; };
      auto it1 = std::lower_bound( _statEvents.begin(), _statEvents.end(), firstEv, cmp );
      auto it2 = std::upper_bound( _statEvents.begin(), _statEvents.end(), lastEv, cmp );

      if( it2 != _statEvents.end() ) ++it2;

      if( it1 == it2 ) return messages; // Nothing to draw here

      const float windowWidthPxl = ImGui::GetWindowWidth();
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      for( ; it1 != it2; ++it1 )
      {
         const TimeStamp localTime = it1->time - globalStartTime;
         const auto posPxlX = cyclesToPxl<float>(
            windowWidthPxl,
            tinfo.duration,
            localTime - tinfo.relativeStartTime );
         drawList->AddCircle( ImVec2(posPxlX, 350.0f), 5.0f, 0XFF0000FF );
      }

      return messages;
   }

   void TimelineStats::addStatEvents( const std::vector<StatEvent>& statEvents )
   {
      _statEvents.insert( _statEvents.end(), statEvents.begin(), statEvents.end() );
   }
}