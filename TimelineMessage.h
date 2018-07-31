#ifndef TIMELINE_MESSAGE_H_
#define TIMELINE_MESSAGE_H_

#include <Hop.h>

namespace hop
{
   enum class TimelineMessageType
   {
      FRAME_TO_TIME,
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

#endif // TIMELINE_MESSAGE_H_
