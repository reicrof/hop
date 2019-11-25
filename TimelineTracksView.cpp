#include "common/Profiler.h"
#include "common/TimelineTrack.h"
#include "common/StringDb.h"

#include "Cursor.h"
#include "TimelineTracksView.h"
#include "TimelineInfo.h"
#include "Options.h"

#include "imgui/imgui.h"

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT       = 20.0f;
static constexpr uint32_t DISABLED_COLOR         = 0xFF505050;
static constexpr uint32_t SEPARATOR_COLOR        = 0xFF666666;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR = 0xFFAAAAAA;

static bool hidden( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
   return tdi.drawInfos[idx].trackHeight <= -tdi.paddedTraceHeight;
}

static float heightWithThreadLabel( const hop::TimelineTrackDrawInfo& tdi, int idx )
{
    return tdi.drawInfos[idx].trackHeight + THREAD_LABEL_HEIGHT;
}

static bool drawSeparator( uint32_t threadIndex, bool highlightSeparator )
{
   const float Y_PADDING   = 5.0f;
   const float windowWidth = ImGui::GetWindowWidth();
   const float handleWidth = 0.05f * windowWidth;
   const float drawPosY    = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y - Y_PADDING;
   ImVec2 p1               = ImGui::GetWindowPos();
   p1.y += drawPosY;

   ImVec2 p2( p1.x + windowWidth, p1.y + 1 );
   ImVec2 handleP2( p2.x - handleWidth, p2.y );

   const ImVec2 mousePos = ImGui::GetMousePos();
   const bool handledHovered =
       std::abs( mousePos.y - p1.y ) < 10.0f && mousePos.x > handleP2.x && threadIndex > 0;

   uint32_t color       = SEPARATOR_COLOR;
   uint32_t handleColor = SEPARATOR_HANDLE_COLOR;
   if( handledHovered && highlightSeparator )
   {
      hop::setCursor( hop::CURSOR_SIZE_NS );
      color = handleColor = 0xFFFFFFFF;
   }

   ImDrawList* drawList = ImGui::GetWindowDrawList();
   drawList->AddLine( p1, p2, color, 3.0f );
   drawList->AddLine( p2, handleP2, handleColor, 6.0f );

   ImGui::SetCursorPosY( ImGui::GetCursorPosY() );

   return handledHovered && highlightSeparator;
}

static void drawLabels(
    hop::TimelineTrackDrawInfo& info,
    const ImVec2& drawPosition,
    const char* threadName,
    uint32_t trackIndex/*,
    const hop::TimelineTracksDrawInfo& info*/ )
{
   const ImVec2 threadLabelSize = ImGui::CalcTextSize( threadName );
   ImGui::PushClipRect(
       ImVec2( drawPosition.x + threadLabelSize.x + 8, drawPosition.y ),
       ImVec2( 99999.0f, 999999.0f ),
       true );

   const bool trackHidden = hidden( info, trackIndex );
   const auto& zoneColors = hop::g_options.zoneColors;
   uint32_t threadLabelCol = zoneColors[( trackIndex + 1 ) % HOP_MAX_ZONE_COLORS];
   if( trackHidden )
   {
      threadLabelCol = DISABLED_COLOR;
   }
   else if( hop::g_options.showCoreInfo )
   {
      // Draw the core labels
      // drawCoresLabels(
      //     ImVec2( drawPosition.x, drawPosition.y + 1.0f ), tracks[trackIndex]._coreEvents, info );
      // Restore draw position for thread label
      //ImGui::SetCursorScreenPos( drawPosition );
   }

   ImGui::PopClipRect();

   // Draw thread label
   ImGui::PushID( trackIndex );
   ImGui::PushStyleColor( ImGuiCol_Button, threadLabelCol );
   if( ImGui::Button( threadName, ImVec2( 0, THREAD_LABEL_HEIGHT ) ) )
   {
      info.drawInfos[trackIndex].trackHeight = trackHidden ? 99999.0f : -99999.0f;
   }
   ImGui::PopStyleColor();
   ImGui::PopID();
}

void hop::drawTimelineTracks( TimelineTrackDrawInfo& info, TimelineMsgArray* msgArray )
{
   //drawTraceDetailsWindow( info, timelineActions );
   //drawSearchWindow( info, timelineActions );
   //drawTraceStats( _traceStats, info.strDb, info.timeline.useCycles );

   ImGui::SetCursorScreenPos( ImVec2( info.timeline.canvasPosX, info.timeline.canvasPosY ) );

   // Get data from profiler
   const std::vector<TimelineTrack>& timelineTracksData = info.profiler.timelineTracks();
   const StringDb& stringDb                             = info.profiler.stringDb();

   char threadNameBuffer[128] = "Thread ";
   const size_t threadNamePrefix = sizeof( "Thread" );
   const float timelineOffsetY = info.timeline.canvasPosY + info.timeline.scrollAmount;
   assert( timelineTracksData.size() == info.drawInfos.size() );
   for ( size_t i = 0; i < info.drawInfos.size(); ++i )
   {
      // Skip empty threads
      const TimelineTrack& trackData = timelineTracksData[i];
      if( trackData.empty() ) continue;

      const bool threadHidden = hidden( info, i );
      const float trackHeight = heightWithThreadLabel( info, i );

      const char* threadLabel = &threadNameBuffer[0];
      hop::StrPtr_t trackname = trackData.name();
      if( trackname != 0 )
      {
         const size_t stringIdx = stringDb.getStringIndex( trackname );
         threadLabel = stringDb.getString( stringIdx );
      }
      else
      {
         snprintf(
             threadNameBuffer + threadNamePrefix, sizeof( threadNameBuffer ) - threadNamePrefix, "%lu", i );
      }
      HOP_PROF_DYN_NAME( threadLabel );

      // First draw the separator of the track
      const bool highlightSeparator = ImGui::IsRootWindowOrAnyChildFocused();
      const bool separatorHovered = drawSeparator( i, highlightSeparator );

      const ImVec2 labelsDrawPosition = ImGui::GetCursorScreenPos();
      drawLabels( info, labelsDrawPosition, threadLabel, i );

      // Then draw the interesting stuff
      const auto absDrawPos = ImGui::GetCursorScreenPos();
      info.drawInfos[i].absoluteDrawPos[0] = absDrawPos.x;
      info.drawInfos[i].absoluteDrawPos[1] = absDrawPos.y + info.timeline.scrollAmount - timelineOffsetY;
      info.drawInfos[i].localDrawPos[0] = absDrawPos.x;
      info.drawInfos[i].localDrawPos[1] = absDrawPos.y;

      // Handle track resize
      if ( separatorHovered || info.draggedTrack > 0 )
      {
         if ( info.draggedTrack == -1 && ImGui::IsMouseClicked( 0 ) )
         {
            info.draggedTrack = (int)i;
         }
         if( ImGui::IsMouseReleased( 0 ) )
         {
            info.draggedTrack = -1;
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
            // drawTrackHighlight(
            //     curDrawPos.x,
            //     curDrawPos.y - THREAD_LABEL_HEIGHT,
            //     trackHeight + THREAD_LABEL_HEIGHT );

            ImGui::PushClipRect(
                ImVec2( 0.0f, curDrawPos.y ),
                ImVec2( 9999.0f, curDrawPos.y + trackHeight ),
                true );

            // Draw the lock waits (before traces so that they are not hiding them)
            // drawLockWaits( i, curDrawPos.x, curDrawPos.y, info, timelineActions );
            // drawTraces( i, curDrawPos.x, curDrawPos.y, info, timelineActions );

            // drawHighlightedTraces(
            //     _tracks[i]._highlightsDrawData,
            //     _highlightValue );
            // _tracks[i]._highlightsDrawData.clear();

            ImGui::PopClipRect();
         }
      } // !threadHidden

      // Set cursor for next drawing iterations
      curDrawPos.y += trackHeight;
      ImGui::SetCursorScreenPos( curDrawPos );
      //*/
   }

   //drawContextMenu( info );
}