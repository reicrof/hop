#ifndef HOP_CURSOR_H_
#define HOP_CURSOR_H_

namespace hop
{
   enum CursorState
   {
      CURSOR_ARROW,
      CURSOR_SIZE_NS,

      CURSOR_STATE_COUNT
   };

   void initCursors();
   void setCursor( CursorState state );
   void drawCursor();
   void uninitCursors();
}

#endif // HOP_CURSOR_H_