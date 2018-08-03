#ifndef TIMELINE_INFO_H_
#define TIMELINE_INFO_H_

#include <Hop.h>

namespace hop
{
   struct TimelineInfo
   {
      float canvasPosX;
      float canvasPosY;
      float scrollAmount;
      TimeStamp globalStartTime;
      TimeStamp relativeStartTime;
      TimeStamp duration;
      bool mouseDragging;
   };

   enum class TimelineMessageType
   {
      FRAME_TO_TIME,
      FRAME_TO_ABSOLUTE_TIME,
      MOVE_VERTICAL_POS_PXL
   };

   struct TimelineMessage
   {
      TimelineMessageType type;

      union
      {
         struct
         {
            TimeStamp time;
            TimeDuration duration;
            bool pushNavState;
         } frameToTime;

         struct
         {
            float posPxl;
         } verticalPos;
      };
   };
}

#endif // TIMELINE_INFO_H_
