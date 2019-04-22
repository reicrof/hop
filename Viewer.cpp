#include "Viewer.h"

#include "Lod.h"
#include "ModalWindow.h"
#include "Options.h"
#include "Profiler.h"
#include "RendererGL.h"
#include "Stats.h"
#include "Utils.h"

#include "imgui/imgui.h"

extern bool g_run;

static void drawMenuBar()
{
   const char* const menuSaveAsHop = "Save as...";
   const char* const menuOpenHopFile = "Open";
   const char* const menuHelp = "Help";
   const char* menuAction = NULL;

   if ( ImGui::BeginMenuBar() )
   {
      if ( ImGui::BeginMenu( "Menu" ) )
      {
         if ( ImGui::MenuItem( menuSaveAsHop, NULL ) )
         {
            menuAction = menuSaveAsHop;
         }
         if( ImGui::MenuItem( menuOpenHopFile, NULL ) )
         {
           menuAction = menuOpenHopFile;
         }
         if( ImGui::MenuItem( menuHelp, NULL ) )
         {
            menuAction = menuHelp;
         }
         if( ImGui::MenuItem( "Options", NULL ) )
         {
            hop::g_options.optionWindowOpened = true;
         }
         ImGui::Separator();
         if ( ImGui::MenuItem( "Exit", NULL ) )
         {
            g_run = false;
         }
         ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
   }

   if ( menuAction == menuSaveAsHop )
   {
      ImGui::OpenPopup( menuSaveAsHop );
   }
   else if( menuAction == menuOpenHopFile )
   {
      ImGui::OpenPopup( menuOpenHopFile );
   }
   else if( menuAction == menuHelp )
   {
      ImGui::OpenPopup( menuHelp );
   }

   if ( ImGui::BeginPopupModal( menuSaveAsHop, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      static char path[512] = {};
      bool shouldSave = ImGui::InputText(
          "Save to",
          path,
          sizeof( path ),
          ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );
      ImGui::Separator();

      if ( ImGui::Button( "Save", ImVec2( 120, 0 ) ) || shouldSave )
      {
         // TODO
         //saveToFile( path );
         ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if ( ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }
   
   if( ImGui::BeginPopupModal( menuOpenHopFile, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      static char path[512] = {};
      const bool shouldOpen = ImGui::InputText(
          "Open file",
          path,
          sizeof( path ),
          ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );

      ImGui::Separator();

      if ( ImGui::Button( "Open", ImVec2( 120, 0 ) ) || shouldOpen )
      {
         // TODO
         //openFile( path );
         ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if ( ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }

   if( ImGui::BeginPopupModal( menuHelp, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      ImGui::Text("Hop version %.1f\n",HOP_VERSION);
      static bool rdtscpSupported = hop::supportsRDTSCP();
      static bool constantTscSupported = hop::supportsConstantTSC();
      ImGui::Text( "RDTSCP Supported : %s\nConstant TSC Supported : %s\n",
                   rdtscpSupported ? "yes" : "no", constantTscSupported ? "yes" : "no" );
      if ( ImGui::Button( "Close", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
   }
}

static void drawFloatingWindows()
{
   if( hop::g_options.debugWindow )
   {
      hop::drawStatsWindow( hop::g_stats );
   }
   drawOptionsWindow( hop::g_options );
}

namespace hop
{
Viewer::Viewer( uint32_t screenSizeX, uint32_t /*screenSizeY*/ )
    : _lastFrameTime( ClockType::now() ), _vsyncEnabled( hop::g_options.vsyncOn )
{
   hop::setupLODResolution( screenSizeX );
   renderer::createResources();
}

void Viewer::addNewProfiler( const char* processName, bool startRecording )
{
   _profilers.emplace_back( new hop::Profiler( processName ) );
   _profilers.back()->setRecording( startRecording );
}

void Viewer::fetchClientsData()
{
   for( auto& p : _profilers )
   {
      p->fetchClientData();
   }
}

void Viewer::onNewFrame(
    int width,
    int height,
    int mouseX,
    int mouseY,
    bool lmbPressed,
    bool rmbPressed,
    float mousewheel )
{
   ImGuiIO& io = ImGui::GetIO();

   // Setup display size (every frame to accommodate for window resizing)
   io.DisplaySize = ImVec2( (float)width, (float)height );
   // io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ?
   // ((float)display_h / h) : 0);

   // Setup time step
   const auto curTime = ClockType::now();
   const float deltaTime = static_cast<float>(
       std::chrono::duration_cast<std::chrono::milliseconds>( ( curTime - _lastFrameTime ) )
           .count() );
   io.DeltaTime = std::max( deltaTime * 0.001f, 0.00001f );  // ImGui expect seconds
   _lastFrameTime = curTime;

   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
   io.MousePos = ImVec2( (float)mouseX, (float)mouseY );

   io.MouseDown[0] = lmbPressed;
   io.MouseDown[1] = rmbPressed;
   io.MouseWheel = mousewheel;

   // Reset frame stats
   hop::g_stats.drawingTimeMs = 0.0;
   hop::g_stats.traceDrawingTimeMs = 0.0;
   hop::g_stats.coreDrawingTimeMs = 0.0;
   hop::g_stats.lockwaitsDrawingTimeMs = 0.0;

   // Set vsync if it has changed.
   if( _vsyncEnabled != hop::g_options.vsyncOn )
   {
      _vsyncEnabled = hop::g_options.vsyncOn;
      renderer::setVSync( _vsyncEnabled );
   }

   // Start the frame
   ImGui::NewFrame();
}

void Viewer::draw( uint32_t windowWidth, uint32_t windowHeight )
{
   ImGui::SetNextWindowPos( ImVec2( 0.0f, 0.0f ), ImGuiCond_Always );
   ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize, ImGuiCond_Always );
   ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );

   ImGui::Begin(
       "Hop Viewer",
       nullptr,
       ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar |
           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar |
           ImGuiWindowFlags_NoResize );

   // Reset the style var so the floating windows can be drawn properly
   ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
   ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 5.0f );

   drawMenuBar();

   ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable;
   if( ImGui::BeginTabBar( "MyTabBar", tab_bar_flags ) )
   {
      if( ImGui::BeginTabItem( "+" ) )
      {
         ImGui::EndTabItem();
      }
      if( ImGui::BeginTabItem( "Avocado" ) )
      {
         ImGui::Text( "This is the Avocado tab!\nblah blah blah blah blah" );
         ImGui::EndTabItem();
      }
      if( ImGui::BeginTabItem( "Broccoli" ) )
      {
         ImGui::Text( "This is the Broccoli tab!\nblah blah blah blah blah" );
         ImGui::EndTabItem();
      }
      if( ImGui::BeginTabItem( "Cucumber" ) )
      {
         ImGui::Text( "This is the Cucumber tab!\nblah blah blah blah blah" );
         ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
   }

   const float dtTimeMs = ImGui::GetIO().DeltaTime * 1000;
   for( auto& p : _profilers )
   {
      p->update( dtTimeMs );
   }

   renderer::setViewport( 0, 0, windowWidth, windowHeight );
   renderer::clearColorBuffer();

   for( auto& p : _profilers )
   {
      p->draw( windowWidth, windowHeight );
   }

   // Render modal window, if any
   if( modalWindowShowing() )
   {
      renderModalWindow();
   }

   ImGui::End(); // Window
   ImGui::PopStyleVar(1);

   handleHotkey();
   drawFloatingWindows();

   // Create the draw commands
   ImGui::Render();

   // Do the actual render
   renderer::renderDrawlist( ImGui::GetDrawData() );
}

bool Viewer::handleHotkey()
{
   if( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Escape ) ) && !modalWindowShowing() )
   {
      hop::displayModalWindow( "Exit ?", hop::MODAL_TYPE_YES_NO, [&]() { g_run = false; } );
      return true;
   }

   return false;
}

Viewer::~Viewer()
{

}

} // namespace hop