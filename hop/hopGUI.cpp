#define HOP_IMPLEMENTATION
#include "Hop.h"
#include "hop/Stats.h"
#include "hop/Options.h"
#include "hop/Renderer.h"
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
   io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;  // Keyboard mapping. ImGui will use those indices to peek
                                        // into the io.KeyDown[] array.
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
   while ( g_run && SDL_PollEvent( &event ) )
   {
      switch ( event.type )
      {
         case SDL_QUIT:
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
            int key = event.key.keysym.sym & ~SDLK_SCANCODE_MASK;
            io.KeysDown[key] = ( event.type == SDL_KEYDOWN );
            io.KeyShift = ( ( SDL_GetModState() & KMOD_SHIFT ) != 0 );
#ifdef __APPLE__
            io.KeyCtrl = ( ( SDL_GetModState() & KMOD_GUI ) != 0 );
#else
            io.KeyCtrl = ( ( SDL_GetModState() & KMOD_CTRL ) != 0 );
#endif
            io.KeyAlt = ( ( SDL_GetModState() & KMOD_ALT ) != 0 );
            io.KeySuper = ( ( SDL_GetModState() & KMOD_GUI ) != 0 );
            break;
         }

         default:
            break;
      }
   }
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

      const auto startFetch = frameStart;
      viewer.fetchClientsData();
      const auto endFetch = ClockType::now();
      hop::g_stats.fetchTimeMs =
          duration<double, std::milli>( ( endFetch - startFetch ) ).count();

      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );

      // Poll SDL events
      handleInput();

      const uint32_t buttonState = SDL_GetMouseState( &x, &y );
      const bool lmb = buttonState & SDL_BUTTON( SDL_BUTTON_LEFT );
      const bool rmb = buttonState & SDL_BUTTON( SDL_BUTTON_RIGHT );

      // Get delta time for current frame
      const auto curTime = ClockType::now();
      const float deltaTime = (float)(
         duration_cast<milliseconds>( ( curTime - lastFrameTime ) ).count() );

      const float wndWidth = (float)w;
      const float wndHeight = (float)h;
      viewer.onNewFrame( deltaTime, wndWidth, wndHeight, x, y, lmb, rmb, g_mouseWheel );

      g_mouseWheel = 0;
      lastFrameTime = curTime;

      viewer.draw( wndWidth, wndHeight);

      auto frameEnd = ClockType::now();
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

   SDL_SetHint( SDL_HINT_RENDER_DRIVER, renderer::sdlRenderDriverHint() );

   if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
   {
      fprintf( stderr, "Failed SDL initialization : %s \n", SDL_GetError() );
      return -1;
   }

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

   if( !renderer::initialize( window ) )
   {
      fprintf( stderr, "FATAL ERROR ! Renderer initializaiton failed\n" );
      return -1;
   }

   const hop::LaunchOptions opts = hop::parseArgs( argc, argv );
   hop::options::load();

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

   destroyIcon();
   ImGui::DestroyContext();
   renderer::terminate();
   hop::block_allocator::terminate();

   SDL_DestroyWindow( window );
   SDL_Quit();
}
