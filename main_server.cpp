#include <server.h>
#include <message.h>
#include <SDL2/SDL.h>

#include <imdbg.h>

#include <rapidjson/document.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

static bool g_run = true;
static float g_mouseWheel = 0.0f;

static void handleMouseWheel( const SDL_Event& e )
{
   if ( e.wheel.y == 1 )  // scroll up
   {
      g_mouseWheel = 1.0f;
   }
   else if ( e.wheel.y == -1 )  // scroll down
   {
      g_mouseWheel = -1.0f;
   }
}

static void handleInput()
{
   SDL_Event event;
   ImGuiIO& io = ImGui::GetIO();
   while ( g_run && SDL_PollEvent( &event ) )
   {
      handleMouseWheel( event );
      switch ( event.type )
      {
         case SDL_QUIT:
            g_run = false;
            break;
         case SDL_TEXTINPUT:
         {
            io.AddInputCharactersUTF8( event.text.text );
            break;
         }
         case SDL_KEYDOWN:
         case SDL_KEYUP:
         {
            if ( event.key.keysym.sym == SDLK_ESCAPE )
            {
               g_run = false;
               break;
            }
            int key = event.key.keysym.sym & ~SDLK_SCANCODE_MASK;
            io.KeysDown[key] = ( event.type == SDL_KEYDOWN );
            io.KeyShift = ( ( SDL_GetModState() & KMOD_SHIFT ) != 0 );
            io.KeyCtrl = ( ( SDL_GetModState() & KMOD_CTRL ) != 0 );
            io.KeyAlt = ( ( SDL_GetModState() & KMOD_ALT ) != 0 );
            io.KeySuper = ( ( SDL_GetModState() & KMOD_GUI ) != 0 );
            break;
         }

         default:
            break;
      }
   }
}

int main()
{
   if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
   {
      printf( "Failed SDL initialization : %s \n", SDL_GetError() );
      return -1;
   }

   SDL_Window* window = SDL_CreateWindow(
       "vdbg", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 1024, SDL_WINDOW_OPENGL );

   if ( window == NULL )
   {
      printf( "Could not create window: %s\n", SDL_GetError() );
      return -1;
   }

   SDL_SetWindowResizable( window, SDL_TRUE );
   SDL_GLContext mainContext = SDL_GL_CreateContext( window );
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetSwapInterval(1);

   imdbg::init();

   vdbg::Server serv;
   serv.start( vdbg::SERVER_PATH, 10 );

   rapidjson::Document doc;

   bool show_test_window = true;

   std::vector< imdbg::Profiler* > profilers;
   std::vector< uint32_t > profTrheadsId;

   while ( g_run )
   {
      handleInput();

      std::vector< vdbg::DisplayableTraceFrame > pendingTraces;
      serv.getProfilingTraces( pendingTraces );
      for ( auto& t : pendingTraces )
      {
         size_t i = 0;
         bool found = false;
         for( ; i < profTrheadsId.size(); ++i )
         {
            if( profTrheadsId[i] == t.threadId )
            {
               found = true;
               break;
            }
         }

         if( found )
         {
            profilers[i]->addTraces( t );
         }
         else
         {
            profilers.push_back(
                imdbg::newProfiler( "Profiler : Thread  " + std::to_string( t.threadId ) ) );
            profTrheadsId.push_back( t.threadId );
            profilers.back()->addTraces( std::move( t ) );
            printf( "THREAD ADDED %u\n", t.threadId );
         }
      }

      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );
      uint32_t buttonState = SDL_GetMouseState( &x, &y );

      printf( "before frame%f\n", g_mouseWheel );
      imdbg::onNewFrame(
          w,
          h,
          x,
          y,
          buttonState & SDL_BUTTON( SDL_BUTTON_LEFT ),
          buttonState & SDL_BUTTON( SDL_BUTTON_RIGHT ),
          g_mouseWheel );
      g_mouseWheel = 0;

      ImGui::ShowTestWindow( &show_test_window );

      glViewport( 0, 0, w, h );
      glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
      glClear( GL_COLOR_BUFFER_BIT );

      imdbg::draw();

      SDL_GL_SwapWindow( window );
   }

   serv.stop();

   SDL_GL_DeleteContext( mainContext );
   SDL_DestroyWindow( window );
   SDL_Quit();
}
