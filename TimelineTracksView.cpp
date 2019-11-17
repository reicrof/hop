#include "common/Profiler.h"
#include "common/TimelineTrack.h"

#include "TimelineTracksView.h"
#include "TimelineInfo.h"

#include "imgui/imgui.h"

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT = 20.0f;

static bool hidden( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
   return tdi.drawInfos[idx].trackHeight <= -tdi.paddedTraceHeight;
}

static float heightWithThreadLabel( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
    return tdi.drawInfos[idx].trackHeight + THREAD_LABEL_HEIGHT;
}

void hop::drawTimelineTracks( const TimelineTrackDrawInfo& info, TimelineMsgArray* msgArray )
{
   //drawTraceDetailsWindow( info, timelineActions );
   //drawSearchWindow( info, timelineActions );
   //drawTraceStats( _traceStats, info.strDb, info.timeline.useCycles );

   ImGui::SetCursorScreenPos( ImVec2( info.timeline.canvasPosX, info.timeline.canvasPosY ) );


   char threadNameBuffer[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread" );
   const float timelineOffsetY = info.timeline.canvasPosY + info.timeline.scrollAmount;
   for ( size_t i = 0; i < info.drawInfos.size(); ++i )
   {
      // Skip empty threads
      //if( _tracks[i].empty() ) continue;

      const bool threadHidden = hidden( info, i );
      const float trackHeight = heightWithThreadLabel( info, i );

      const TimelineTrack& curTrack = info.profiler.timelineTrackAt( i );
      const char* threadName = &threadNameBuffer[0];
      if( curTrack.name() != 0 )
      {
         //const size_t stringIdx = info.strDb.getStringIndex( curTrack.name() );
         //threadName = info.strDb.getString( stringIdx );
      }
      else
      {
         snprintf(
             threadNameBuffer + threadNamePrefix, sizeof( threadNameBuffer ) - threadNamePrefix, "%lu", i );
      }
      HOP_PROF_DYN_NAME( threadName );

/*
      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
      drawLabels( labelsDrawPosition, threadName, _tracks, i, info );

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      _tracks[i]._absoluteDrawPos[0] = absDrawPos.x;
      _tracks[i]._absoluteDrawPos[1] = absDrawPos.y + info.timeline.scrollAmount - timelineOffsetY;
      _tracks[i]._localDrawPos[0] = absDrawPos.x;
      _tracks[i]._localDrawPos[1] = absDrawPos.y;

      // Handle track resize
      if ( separatorHovered || _draggedTrack > 0 )
      {
         if ( _draggedTrack == -1 && ImGui::IsMouseClicked( 0 ) )
         {
            _draggedTrack = (int)i;
         }
         if( ImGui::IsMouseReleased( 0 ) )
         {
            _draggedTrack = -1;
         }
      }

      ImVec2 curDrawPos = absDrawPos;
      if (!threadHidden)
      {
         const float threadStartRelDrawPos = curDrawPos.y - ImGui::GetWindowPos().y;
         const float threadEndRelDrawPos = threadStartRelDrawPos + trackHeight;

         const bool tracesVisible =
             !( threadStartRelDrawPos > ImGui::GetWindowHeight() || threadEndRelDrawPos < 0 );

         if( tracesVisible )
         {
            // Track highlights needs to be drawn before the traces themselves as it acts as a background
            drawTrackHighlight(
                curDrawPos.x,
                curDrawPos.y - THREAD_LABEL_HEIGHT,
                trackHeight + THREAD_LABEL_HEIGHT );

            ImGui::PushClipRect(
                ImVec2( 0.0f, curDrawPos.y ),
                ImVec2( 9999.0f, curDrawPos.y + trackHeight ),
                true );

            // Draw the lock waits (before traces so that they are not hiding them)
            drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            drawTraces( i, curDrawPos.x, curDrawPos.y, info, timelineActions );

            drawHighlightedTraces(
                _tracks[i]._highlightsDrawData,
                _highlightValue );
            _tracks[i]._highlightsDrawData.clear();

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
      */
   }

   //drawContextMenu( info );
}