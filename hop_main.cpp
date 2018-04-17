#define HOP_IMPLEMENTATION
#include <Hop.h>
#include "Profiler.h"
#include "Stats.h"
#include "imgui/imgui.h"
#include "argtable3.h"
#include "Options.h"
#include <SDL.h>
#undef main

#include <signal.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifndef _MSC_VER
#include <sys/wait.h>
#endif

static bool g_run = true;
static float g_mouseWheel = 0.0f;

void terminateCallback( int sig )
{
   signal( sig, SIG_IGN );
   g_run = false;
}

const char* (*GetClipboardTextFn)(void* user_data);
void(*SetClipboardTextFn)(void* user_data, const char* text);

static const char* getClipboardText(void*)
{
   return SDL_GetClipboardText();
}

static void setClipboardText(void*, const char* text)
{
   SDL_SetClipboardText(text);
}

static void sdlImGuiInit()
{
   ImGui::CreateContext();

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

   io.SetClipboardTextFn = setClipboardText;
   io.GetClipboardTextFn = getClipboardText;

   auto& style = ImGui::GetStyle();
   style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
   style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
   style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
   style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
   style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
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

#if defined( _MSC_VER )
typedef HANDLE processId_t;
#else
typedef int processId_t;
#endif

static processId_t startChildProcess( const char* path, const char* basename )
{
   processId_t newProcess = 0;
#if defined( _MSC_VER )
   STARTUPINFO si = {0};
   PROCESS_INFORMATION pi = {0};

   si.cb = sizeof( si );
   (void)basename;
   if ( !CreateProcess( NULL, (LPSTR)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) )
   {
      printErrorMsg("Error starting executable");
      return false;
   }
   newProcess = pi.hProcess;
#else
   newProcess = fork();
   if ( newProcess == 0 )
   {
      char* processName = strdup( basename );
      char* const subprocessArg[] = {processName, nullptr};
      int res = execvp( path, subprocessArg );
      if ( res < 0 )
      {
         printErrorMsg( "Error starting executable" );
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
      while ((wpid = wait(&status)) > 0);
   }
#endif
}

struct ArgTableCleanerGuard
{
   ArgTableCleanerGuard( void** table, int count ) : _table(table), _count(count) {}
   ~ArgTableCleanerGuard()
   {
      arg_freetable( _table, _count );
   }
   void** _table;
   int _count;
};

struct arg_lit *help, *version;
struct arg_file *exec, *input;
struct arg_end *end;

int main( int argc, char* argv[] )
{
   char progname[] = "hop";
   void* argtable[] = {
       input    = arg_filen(nullptr, nullptr, "<process name>", 0, 1, "Starts listening for specified process"),
       exec      = arg_filen("eE", "execute", "executable", 0, 1, "Launch specified executable and start recording"),
       help      = arg_litn( "h", "help", 0, 1, "Display this help and exit" ),
       version   = arg_litn( "v", "version", 0, 1, "Display version info and exit" ),
       end       = arg_end( 10 ),
   };
   ArgTableCleanerGuard cleaner( argtable, sizeof( argtable ) / sizeof( argtable[0] ) );

   int nerrors = arg_parse( argc, argv, argtable );

   /* special case: '--help' takes precedence over error reporting */
   if ( help->count > 0 )
   {
      printf( "Usage: %s", progname );
      arg_print_syntax( stdout, argtable, "\n" );
      printf( "Available hop arguments : \n\n" );
      arg_print_glossary( stdout, argtable, "  %-25s %s\n" );
      
      return 0;
   }

   /* If the parser returned any errors then display them and exit */
   if ( nerrors > 0 )
   {
      /* Display the error details contained in the arg_end struct.*/
      arg_print_errors( stdout, end, progname );
      printf( "Try '%s --help' for more information.\n", progname );
      return -1;
   }

   if( version->count > 0 )
   {
      printf("hop version %.1f \n", HOP_VERSION );
      return 0;
   }

   const char* executablePath = nullptr;
   const char* executableName = nullptr;
   processId_t childProcess = 0;

   if( exec->count > 0 )
   {
      executablePath = exec->filename[0];
      executableName = exec->basename[0];
   }
   else if( input->count > 0 )
   {
      executableName = input->filename[0];
   }
   else
   {
      arg_print_glossary( stdout, argtable, "  %-30s %s\n" );
      return 0;
   }

   // Setup signal handlers
   signal( SIGINT, terminateCallback );
   signal( SIGTERM, terminateCallback );
#ifndef _MSC_VER
   signal( SIGCHLD, SIG_IGN );
#endif

   if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
   {
      printf( "Failed SDL initialization : %s \n", SDL_GetError() );
      return -1;
   }

   hop::loadOptions();

   uint32_t createWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
   if( hop::g_options.startFullScreen ) createWindowFlags |= SDL_WINDOW_MAXIMIZED;

   SDL_Window* window = SDL_CreateWindow(
       "Hop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 1024, createWindowFlags );

   if ( window == NULL )
   {
      printf( "Could not create window: %s\n", SDL_GetError() );
      return -1;
   }

   sdlImGuiInit();

   SDL_GLContext mainContext = SDL_GL_CreateContext( window );
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetSwapInterval(1);

   hop::init();

   auto profiler = std::unique_ptr< hop::Profiler >( new hop::Profiler( executableName ) );
   hop::addNewProfiler( profiler.get() );

   // If we want to launch an executable to profile, now is the time to do it
   if( executablePath )
   {
      childProcess = startChildProcess(exec->filename[0], executableName);
      if( childProcess == 0 )
      {
         exit(-1);
      }

      // Try to start recording until the shared memory is created
      while( processAlive( childProcess ) && !profiler->setRecording( true ) )
      {;}
   }

   while ( g_run )
   {
      const auto frameStart = std::chrono::system_clock::now();
      handleInput();

      const auto startFetch = std::chrono::system_clock::now();
      profiler->fetchClientData();
      const auto endFetch = std::chrono::system_clock::now();
      hop::g_stats.fetchTimeMs = std::chrono::duration< double, std::milli>( ( endFetch - startFetch ) ).count();


      int w, h, x, y;
      SDL_GetWindowSize( window, &w, &h );
      uint32_t buttonState = SDL_GetMouseState( &x, &y );

      const auto drawStart = std::chrono::system_clock::now();

      hop::onNewFrame(
          w,
          h,
          x,
          y,
          buttonState & SDL_BUTTON( SDL_BUTTON_LEFT ),
          buttonState & SDL_BUTTON( SDL_BUTTON_RIGHT ),
          g_mouseWheel );
      g_mouseWheel = 0;

      glViewport( 0, 0, w, h );
      glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
      glClear( GL_COLOR_BUFFER_BIT );

      hop::draw();

      const auto drawEnd = std::chrono::system_clock::now();
      hop::g_stats.drawingTimeMs = std::chrono::duration< double, std::milli>( ( drawEnd - drawStart ) ).count();

      SDL_GL_SwapWindow( window );

      const auto frameEnd = std::chrono::system_clock::now();
      hop::g_stats.frameTimeMs = std::chrono::duration< double, std::milli>( ( frameEnd - frameStart ) ).count();
   }

   hop::saveOptions();

   // We have launched a child process. Let's close it
   if( executablePath )
   {
      terminateProcess( childProcess );
   }

   ImGui::DestroyContext();

   SDL_GL_DeleteContext( mainContext );
   SDL_DestroyWindow( window );
   SDL_Quit();
}
