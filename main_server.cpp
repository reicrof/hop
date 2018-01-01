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

static void sdlImGuiInit()
{
  
  ImGuiIO& io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_Tab] = SDLK_TAB; // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
  io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
  io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
  io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
  io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
  io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
  io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
  io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
  io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
  io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
  io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
  io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
  io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
  io.KeyMap[ImGuiKey_A] = SDLK_a;
  io.KeyMap[ImGuiKey_C] = SDLK_c;
  io.KeyMap[ImGuiKey_V] = SDLK_v;
  io.KeyMap[ImGuiKey_X] = SDLK_x;
  io.KeyMap[ImGuiKey_Y] = SDLK_y;
  io.KeyMap[ImGuiKey_Z] = SDLK_z;
}

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

   sdlImGuiInit();

   SDL_SetWindowResizable( window, SDL_TRUE );
   SDL_GLContext mainContext = SDL_GL_CreateContext( window );
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetSwapInterval(1);

   vdbg::init();

   vdbg::Server serv;
   serv.start( vdbg::SERVER_PATH, 10 );

   rapidjson::Document doc;

   bool show_test_window = true;

   auto profiler = std::make_unique< vdbg::Profiler >( "Profiler Test" );
   vdbg::addNewProfiler( profiler.get() );
   std::vector< uint32_t > profTrheadsId;

   std::vector< uint32_t > threadIds;
   std::vector< std::vector< vdbg::DisplayableTrace > > pendingTraces;
   std::vector< std::vector< char > > stringData;
   while ( g_run )
   {
      handleInput();

      serv.getProfilingTraces( pendingTraces, stringData, threadIds );
      for( size_t i = 0; i < pendingTraces.size(); ++i )
      {
         profiler->addTraces( pendingTraces[i], threadIds[i] );
         profiler->setStringData( stringData[i], threadIds[i] );
      }

      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );
      uint32_t buttonState = SDL_GetMouseState( &x, &y );

      vdbg::onNewFrame(
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

      vdbg::draw();

      SDL_GL_SwapWindow( window );
   }

   serv.stop();

   SDL_GL_DeleteContext( mainContext );
   SDL_DestroyWindow( window );
   SDL_Quit();
}
