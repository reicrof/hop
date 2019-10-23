#ifndef TIMELINE_INFO_H_
#define TIMELINE_INFO_H_

#include <Hop.h>

#include <cassert>

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
      MOVE_TO_PRESENT_TIME,
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
         bool withAnimation;
      };

      TimelineMessageType type;
      union
      {
         FrameToTime frameToTime;
         VerticalPos verticalPos;
      };
   };

   class TimelineMsgArray
   {
      static constexpr int MAX_MSG_COUNT = 16;
      unsigned count;
      TimelineMessage messages[MAX_MSG_COUNT];
   public:
      unsigned size() const { return count; }
      const TimelineMessage& operator[]( unsigned index ) const { return messages[index]; };
      void addFrameTimeMsg( TimeStamp time, TimeDuration duration, bool pushNavState, bool absTime )
      {
         assert( count < MAX_MSG_COUNT );
         TimelineMessage* msg = &messages[count++];
         msg->type            = absTime ? TimelineMessageType::FRAME_TO_ABSOLUTE_TIME
                             : TimelineMessageType::FRAME_TO_TIME;
         msg->frameToTime.time = time;
         msg->frameToTime.duration = duration;
         msg->frameToTime.pushNavState = pushNavState;
      }

      void addMoveVerticalPositionMsg( float posPxl, bool withAnimation )
      {
         assert( count < MAX_MSG_COUNT );
         TimelineMessage* msg = &messages[count++];
         msg->type = TimelineMessageType::MOVE_VERTICAL_POS_PXL;
         msg->verticalPos.posPxl = posPxl;
         msg->verticalPos.withAnimation = withAnimation;
      }

      void addMoveToPresentTimeMsg()
      {
         assert( count < MAX_MSG_COUNT );
         TimelineMessage* msg = &messages[count++];
         msg->type = TimelineMessageType::MOVE_TO_PRESENT_TIME;
      }
   };
}

#endif // TIMELINE_INFO_H_
