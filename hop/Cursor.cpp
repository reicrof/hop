#include "hop/Cursor.h"

#include <SDL.h>

static SDL_Cursor* cursors[hop::CURSOR_STATE_COUNT];
static hop::CursorState cursorState = hop::CURSOR_ARROW;

void hop::initCursors()
{
   cursors[0] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
   cursors[1] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
   setCursor( hop::CURSOR_ARROW );
}

void hop::setCursor( CursorState state )
{
   cursorState = state;
}

void hop::drawCursor()
{
   if( SDL_GetCursor() != cursors[cursorState] )
   {
      SDL_SetCursor( cursors[cursorState] );
   }
}

void hop::uninitCursors()
{
   for( auto c : cursors )
   {
      SDL_FreeCursor( c );
   }
}
