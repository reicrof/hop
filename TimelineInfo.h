#ifndef TIMELINE_INFO_H_
#define TIMELINE_INFO_H_

#include <Hop.h>

namespace hop
{
   class StringDb;

   struct TimelineInfo
   {
      float canvasPosX;
      float canvasPosY;
      float scrollAmount;
      TimeStamp globalStartTime;
      TimeStamp relativeStartTime;
      TimeDuration duration;
      bool mouseDragging;
      bool useCycles;
   };

   struct TimelineDrawInfo
   {
      const TimelineInfo& timeline;
      const StringDb& strDb;
   };

   enum class TimelineMessageType
   {
      FRAME_TO_TIME,
      FRAME_TO_ABSOLUTE_TIME,
      MOVE_VERTICAL_POS_PXL
   };

   struct TimelineMessage
   {
      struct FrameToTime
      {
         TimeStamp time;
         TimeDuration duration;
         bool pushNavState;
      };

      struct VerticalPos
      {
         float posPxl;
      };

      TimelineMessageType type;
      union
      {
         FrameToTime frameToTime;
         VerticalPos verticalPos;
      };
   };
}

#endif // TIMELINE_INFO_H_
