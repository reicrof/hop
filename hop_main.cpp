#define HOP_IMPLEMENTATION
#include <Hop.h>
#include "Stats.h"
#include "imgui/imgui.h"
#include "Options.h"
#include "ModalWindow.h"
#include "Cursor.h"
#include "Viewer.h"
#include "Lod.h"
#include <SDL.h>
#undef main

#include "hop_icon_data.inline"
#include "miniz.h"

#include <signal.h>
#include <string>

#ifndef _MSC_VER
#include <sys/wait.h>
#endif

bool g_run = true;
static float g_mouseWheel = 0.0f;
static SDL_Surface* iconSurface = nullptr;

void terminateCallback( int sig )
{
   signal( sig, SIG_IGN );
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

#if defined( _MSC_VER )
typedef HANDLE processId_t;
#else
typedef int processId_t;
#endif

static processId_t startChildProcess( const char* path, char** args )
{
   processId_t newProcess = 0;
#if defined( _MSC_VER )
   STARTUPINFO si = {0};
   PROCESS_INFORMATION pi = {0};

   // TODO Fix arguments passing
   (void)args;
   si.cb = sizeof( si );
   if ( !CreateProcess( NULL, (LPSTR)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) )
   {
      return (processId_t)-1;
   }
   newProcess = pi.hProcess;
#else
   newProcess = fork();
   if ( newProcess == 0 )
   {
      int res = execvp( path, args );
      if ( res < 0 )
      {
         exit( 0 );
      }
   }
#endif
   return newProcess;
}

bool processAlive( processId_t id )
{
#if defined( _MSC_VER )
   DWORD exitCode;
   GetExitCodeProcess( id, &exitCode );
   return exitCode == STILL_ACTIVE;
#else
   return kill( id, 0 ) == 0;
#endif
}

static void terminateProcess( processId_t id )
{
#if defined( _MSC_VER )
   TerminateProcess( id, 0 );
   WaitForSingleObject( id, INFINITE );
   CloseHandle( id );
#else
   if ( processAlive( id ) )
   {
      kill( id, SIGINT );
      int status, wpid;
      while ( ( wpid = wait( &status ) ) > 0 )
         ;
   }
#endif
}

static void printUsage()
{
   printf(
       "Usage : hop [OPTION] <process name>\n\n OPTIONS:\n\t-e Launch specified executable and "
       "start recording\n\t-v Display version info and exit\n\t-h Show usage\n" );
   exit( 0 );
}

struct LaunchOptions
{
   const char* fullProcessPath;
   const char* processName;
   char** args;
   bool startExec;
};

static LaunchOptions createLaunchOptions( char* fullProcessPath, char** argv, bool startExec )
{
   LaunchOptions opts = {fullProcessPath, fullProcessPath, argv, startExec};
   std::string fullPathStr( fullProcessPath );
   size_t lastSeparator = fullPathStr.find_last_of( "/\\" );
   if ( lastSeparator != std::string::npos )
   {
      opts.processName = &fullProcessPath[++lastSeparator];
   }

   return opts;
}

static LaunchOptions parseArgs( int argc, char* argv[] )
{
   if ( argc > 1 )
   {
      if ( argv[1][0] == '-' )
      {
         switch ( argv[1][1] )
         {
            case 'v':
               printf( "hop version %.2f \n", HOP_VERSION );
               exit( 0 );
               break;
            case 'h':
               printUsage();
               exit( 0 );
            case 'e':
               if ( argc > 2 )
               {
                  return createLaunchOptions( argv[2], &argv[2], true );
               }
               // Fallthrough
            default:
               fprintf( stderr, "Invalid arguments\n" );
               break;
         }
      }
      else
      {
         return createLaunchOptions( argv[1], &argv[1], false );
      }
   }
   return LaunchOptions{};
}

int main( int argc, char* argv[] )
{
   const LaunchOptions opts = parseArgs( argc, argv );

   // Setup signal handlers
   signal( SIGINT, terminateCallback );
   signal( SIGTERM, terminateCallback );
#ifndef _MSC_VER
   signal( SIGCHLD, SIG_IGN );
#endif

   if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
   {
      fprintf( stderr, "Failed SDL initialization : %s \n", SDL_GetError() );
      return -1;
   }

   hop::loadOptions();

   uint32_t createWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
   if ( hop::g_options.startFullScreen ) createWindowFlags |= SDL_WINDOW_MAXIMIZED;

   SDL_Window* window = SDL_CreateWindow(
       "Hop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 1024, createWindowFlags );

   if ( window == NULL )
   {
      fprintf( stderr, "Could not create window: %s\n", SDL_GetError() );
      return -1;
   }

   sdlImGuiInit();

   SDL_GLContext mainContext = SDL_GL_CreateContext( window );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
   SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
   SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
   SDL_GL_SetSwapInterval( 1 );

   createIcon( window );

   hop::initCursors();

   // Setup the LOD granularity based on screen resolution
   SDL_DisplayMode DM;
   SDL_GetCurrentDisplayMode( 0, &DM );
   hop::setupLODResolution( DM.w );

   hop::Viewer viewer( DM.w, DM.h );

   processId_t childProcess = 0;
   if ( opts.processName )
   {
      viewer.addNewProfiler( opts.processName, opts.startExec );

      // If we want to launch an executable to profile, now is the time to do it
      if ( opts.startExec )
      {
         // profiler->setRecording( true );
         childProcess = startChildProcess( opts.fullProcessPath, opts.args );
         if ( childProcess == (processId_t)-1 )
         {
            fprintf( stderr, "Could not launch child process\n" );
            exit( -1 );
         }
      }
   }

   while ( g_run )
   {
      const auto frameStart = std::chrono::system_clock::now();

      handleInput();

      const auto startFetch = std::chrono::system_clock::now();
      viewer.fetchClientsData();
      const auto endFetch = std::chrono::system_clock::now();
      hop::g_stats.fetchTimeMs =
          std::chrono::duration<double, std::milli>( ( endFetch - startFetch ) ).count();

      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );
      const uint32_t buttonState = SDL_GetMouseState( &x, &y );
      const bool lmb = buttonState & SDL_BUTTON( SDL_BUTTON_LEFT );
      const bool rmb = buttonState & SDL_BUTTON( SDL_BUTTON_RIGHT );

      // Reset cursor at start of the frame
      hop::setCursor( hop::CURSOR_ARROW );

      viewer.onNewFrame( w, h, x, y, lmb, rmb, g_mouseWheel );
      g_mouseWheel = 0;

      ImGui::ShowDemoWindow();

      viewer.draw( w, h );

      hop::drawCursor();

      const auto frameEnd = std::chrono::system_clock::now();

      // If we rendered fast, fetch data again instead of stalling on the vsync
      if ( std::chrono::duration<double, std::milli>( ( frameEnd - frameStart ) ).count() < 10.0 )
      {
         viewer.fetchClientsData();
      }

      SDL_GL_SwapWindow( window );

      hop::g_stats.frameTimeMs =
          std::chrono::duration<double, std::milli>( ( frameEnd - frameStart ) ).count();
   }

   hop::saveOptions();

   // We have launched a child process. Let's close it
   if ( opts.startExec )
   {
      terminateProcess( childProcess );
   }

   destroyIcon();

   hop::uninitCursors();

   ImGui::DestroyContext();

   SDL_GL_DeleteContext( mainContext );
   SDL_DestroyWindow( window );
   SDL_Quit();
}
