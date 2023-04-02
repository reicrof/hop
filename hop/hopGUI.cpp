#define HOP_IMPLEMENTATION
#include "Hop.h"
#include "hop/Stats.h"
#include "hop/Options.h"
#include "hop/Viewer.h"
#include "hop_icon_raster.inline"

#include "common/miniz.h"
#include "common/BlockAllocator.h"
#include "common/Utils.h"
#include "common/Startup.h"
#include "common/platform/Platform.h"

#include "imgui/imgui.h"
#include <SDL.h>
#undef main

#include <chrono>
#include <string>

using ClockType = std::chrono::steady_clock;

bool g_run = true;
static float g_mouseWheel = 0.0f;
static SDL_Surface* iconSurface = nullptr;

static void terminateCallback( int /*sig*/ )
{
   g_run = false;
}

const char* ( *GetClipboardTextFn )( void* user_data );
void ( *SetClipboardTextFn )( void* user_data, const char* text );

static const char* getClipboardText( void* ) { return SDL_GetClipboardText(); }

static void setClipboardText( void*, const char* text ) { SDL_SetClipboardText( text ); }

static void createIcon( SDL_Window* window )
{
   const uint32_t width = hop_icon.width;
   const uint32_t height = hop_icon.height;
   const uint32_t bytesPerPxl = hop_icon.bytesPerPxl;

   mz_ulong uncompressedSize = width * height * bytesPerPxl + 1;
   std::vector<unsigned char> uncompressedIcon( uncompressedSize );
   int res = mz_uncompress(
       uncompressedIcon.data(),
       &uncompressedSize,
       hop_icon.pixelData,
       sizeof( hop_icon.pixelData ) );
   if ( res == MZ_OK )
   {
      // Load fab icon
      iconSurface = SDL_CreateRGBSurfaceFrom(
          uncompressedIcon.data(),
          width,
          height,
          bytesPerPxl * 8,
          width * bytesPerPxl,
          0x000000ff,
          0x0000ff00,
          0x00ff0000,
          0xff000000 );

      if ( iconSurface != nullptr ) SDL_SetWindowIcon( window, iconSurface );
   }
}

static void destroyIcon()
{
   if ( iconSurface ) SDL_FreeSurface( iconSurface );
}

static void sdlImGuiInit()
{
   ImGui::CreateContext();

   ImGuiIO& io = ImGui::GetIO();

   io.SetClipboardTextFn = setClipboardText;
   io.GetClipboardTextFn = getClipboardText;

   const ImVec4 darkGrey = ImVec4( 0.15f, 0.15f, 0.15f, 1.0f );
   const ImVec4 grey = ImVec4( 0.2f, 0.2f, 0.2f, 1.0f );
   const ImVec4 lightGrey = ImVec4( 0.3f, 0.3f, 0.3f, 1.0f );
   const ImVec4 lightestGrey = ImVec4( 0.45f, 0.45f, 0.45f, 1.0f );

   auto& style = ImGui::GetStyle();
   style.Colors[ImGuiCol_TitleBg] = darkGrey;
   style.Colors[ImGuiCol_WindowBg] = grey;
   style.Colors[ImGuiCol_ChildBg] = grey;
   style.Colors[ImGuiCol_PopupBg] = grey;
   style.Colors[ImGuiCol_MenuBarBg] = lightestGrey;

   style.Colors[ImGuiCol_Button] = lightGrey;
   style.Colors[ImGuiCol_ButtonHovered] = lightestGrey;
   style.Colors[ImGuiCol_ButtonActive] = darkGrey;
   style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 1.0, 1.0, 1.0, 0.1 );
}

static ImGuiKey sdlToImguiKey(int keycode)
{
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
    }
    return ImGuiKey_None;
}

static void handleMouseWheel( const SDL_Event& e )
{
   if ( e.wheel.y > 0 )  // scroll up
   {
      g_mouseWheel = 1.0f;
   }
   else if ( e.wheel.y < 0 )  // scroll down
   {
      g_mouseWheel = -1.0f;
   }
   else
   {
      g_mouseWheel = 0.0f;
   }
}

static void handleInput()
{
   SDL_Event event;
   ImGuiIO& io = ImGui::GetIO();
   bool key_event = false;
   bool window_closing = false;
   while ( g_run && SDL_PollEvent( &event ) )
   {
      switch ( event.type )
      {
         case SDL_QUIT:
#ifdef __APPLE__
            if (key_event && window_closing)
               break;
#endif
            g_run = false;
            break;
         case SDL_MOUSEWHEEL:
            handleMouseWheel( event );
            break;
         case SDL_TEXTINPUT:
         {
            io.AddInputCharactersUTF8( event.text.text );
            break;
         }
         case SDL_KEYDOWN:
         case SDL_KEYUP:
         {
            SDL_Keymod mod = (SDL_Keymod)event.key.keysym.mod;
            io.AddKeyEvent(ImGuiMod_Ctrl, (mod & KMOD_CTRL) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (mod & KMOD_SHIFT) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (mod & KMOD_ALT) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (mod & KMOD_GUI) != 0);

            ImGuiKey key = sdlToImguiKey(event.key.keysym.sym);
            io.AddKeyEvent(key, (event.type == SDL_KEYDOWN));

            key_event = true;
            break;
         }
#ifdef __APPLE__
         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                window_closing = true;
            break;
#endif

         default:
            break;
      }
   }
   HOP_UNUSED( key_event );
   HOP_UNUSED( window_closing );
}

static hop::ProcessID startViewer( SDL_Window* window, const hop::LaunchOptions& opts )
{
   // Setup the LOD granularity based on screen resolution
   SDL_DisplayMode DM;
   SDL_GetCurrentDisplayMode( 0, &DM );

   hop::ProcessID childProcId = -1;
   hop::Viewer viewer( DM.w, DM.h );

   if ( opts.processName )
   {
      // If we want to launch an executable to profile, now is the time to do it
      if ( opts.startExec )
      {
         // profiler->setRecording( true );
         childProcId = hop::startChildProcess( opts.fullProcessPath, opts.args );
         if ( childProcId == (hop::ProcessID)-1 )
         {
            fprintf( stderr, "Could not launch child process\n" );
            exit( -1 );
         }
      }

      // Add new profiler after having potentially started it.
      viewer.addNewProfiler( opts.processName, opts.startExec );
   }

   using namespace std::chrono;
   time_point<ClockType> lastFrameTime = ClockType::now();
   while ( g_run )
   {
      HOP_PROF( "Main Loop" );
      const auto frameStart = ClockType::now();

      handleInput();

      const auto startFetch = ClockType::now();
      viewer.fetchClientsData();
      const auto endFetch = ClockType::now();
      hop::g_stats.fetchTimeMs =
          duration<double, std::milli>( ( endFetch - startFetch ) ).count();

      // Display scaling
      const float scaling = hop::options::displayScaling();
      ImGui::GetIO().DisplayFramebufferScale = ImVec2(scaling, scaling);

      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );
      const uint32_t buttonState = SDL_GetMouseState( &x, &y );
      const bool lmb = buttonState & SDL_BUTTON( SDL_BUTTON_LEFT );
      const bool rmb = buttonState & SDL_BUTTON( SDL_BUTTON_RIGHT );

      // Get delta time for current frame
      const auto curTime = ClockType::now();
      const float deltaTime = static_cast<float>(
         duration_cast<milliseconds>( ( curTime - lastFrameTime ) ).count() );

      const float wndWidth = (float)w/scaling;
      const float wndHeight = (float)h/scaling;
      viewer.onNewFrame( deltaTime, wndWidth, wndHeight, x / scaling, y / scaling, lmb, rmb, g_mouseWheel );

      g_mouseWheel = 0;
      lastFrameTime = curTime;

      viewer.draw( wndWidth, wndHeight);

      auto frameEnd = ClockType::now();

      // If we rendered fast, fetch data again instead of stalling on the vsync
      if ( duration<double, std::milli>( ( frameEnd - frameStart ) ).count() < 10.0 )
      {
         viewer.fetchClientsData();
      }

      SDL_GL_SwapWindow( window );

      frameEnd = ClockType::now();

      hop::g_stats.frameTimeMs = duration<double, std::milli>( ( frameEnd - frameStart ) ).count();
   }

   return childProcId;
}

int main( int argc, char* argv[] )
{
   // Confirm the platform supports HOP
   if ( !hop::verifyPlatform() )
   {
      return -2;
   }

   hop::setupSignalHandlers( terminateCallback );

   if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
   {
      fprintf( stderr, "Failed SDL initialization : %s \n", SDL_GetError() );
      return -1;
   }

   // Needs to be loaded before window creation since it may affect fullscreen/vsync
   hop::options::load();

   uint32_t createWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
   if ( hop::options::fullscreen() ) createWindowFlags |= SDL_WINDOW_MAXIMIZED;

   SDL_Window* window = SDL_CreateWindow(
       "Hop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 1024, createWindowFlags );

   if ( window == NULL )
   {
      fprintf( stderr, "Could not create window: %s\n", SDL_GetError() );
      return -1;
   }

   hop::block_allocator::initialize( hop::VIRT_MEM_BLK_SIZE );

   sdlImGuiInit();

   SDL_GLContext mainContext = SDL_GL_CreateContext( window );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
   SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
   SDL_GL_SetSwapInterval( 1 );

   const hop::LaunchOptions opts = hop::parseArgs( argc, argv );

   createIcon( window );

   HOP_SET_THREAD_NAME( "Main" );

   // Start the viewer and all its profilers
   hop::ProcessID childProcId = startViewer( window, opts );

   hop::options::save();

   // We have launched a child process. Let's close it
   if ( opts.startExec )
   {
      hop::terminateProcess( childProcId );
   }

   hop::block_allocator::terminate();

   destroyIcon();

   ImGui::DestroyContext();

   SDL_GL_DeleteContext( mainContext );
   SDL_DestroyWindow( window );
   SDL_Quit();
}
