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
         if ( ImGui::MenuItem( menuOpenHopFile, NULL ) )
         {
            menuAction = menuOpenHopFile;
         }
         if ( ImGui::MenuItem( menuHelp, NULL ) )
         {
            menuAction = menuHelp;
         }
         if ( ImGui::MenuItem( "Options", NULL ) )
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
   else if ( menuAction == menuOpenHopFile )
   {
      ImGui::OpenPopup( menuOpenHopFile );
   }
   else if ( menuAction == menuHelp )
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
         // saveToFile( path );
         ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if ( ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }

   if ( ImGui::BeginPopupModal( menuOpenHopFile, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
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
         // openFile( path );
         ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if ( ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
   }

   if ( ImGui::BeginPopupModal( menuHelp, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      ImGui::Text( "Hop version %.1f\n", HOP_VERSION );
      static bool rdtscpSupported = hop::supportsRDTSCP();
      static bool constantTscSupported = hop::supportsConstantTSC();
      ImGui::Text(
          "RDTSCP Supported : %s\nConstant TSC Supported : %s\n",
          rdtscpSupported ? "yes" : "no",
          constantTscSupported ? "yes" : "no" );
      if ( ImGui::Button( "Close", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
   }
}

static void drawFloatingWindows()
{
   if ( hop::g_options.debugWindow )
   {
      hop::drawStatsWindow( hop::g_stats );
   }
   drawOptionsWindow( hop::g_options );
}

static const char* displayableProfilerName(hop::Profiler* prof)
{
   const char* profName = prof->execName();
   profName = profName[0] == '\0' ? "<No Target Process>" : profName;
   return profName;
}

static void drawTabs(
    hop::Viewer& viewer,
    int* selectedTab )
{
   static const float MAX_FULL_SIZE_TAB = 8.0f;

   const ImVec2 windowSize = ImGui::GetWindowSize();
   const float tabHeight = 30.0f;
   const float addTabWidth = tabHeight * 1.5;
   const float addTabPadding = windowSize.x * 0.001f;
   const float tabBarWidth = windowSize.x - addTabWidth - addTabPadding;
   const float fullSizeTab = tabBarWidth / MAX_FULL_SIZE_TAB;
   const int profCount = viewer.profilerCount();
   const float tabWidth = std::min( tabBarWidth / profCount, fullSizeTab );
   const uint32_t activeWindowColor = ImGui::GetColorU32( ImGuiCol_WindowBg );
   const uint32_t inactiveWindowColor = activeWindowColor + 0x00303030;
   const float tabFramePadding = 0.1f;
   ImGui::PushStyleColor( ImGuiCol_Border, activeWindowColor );
   ImGui::PushStyleColor( ImGuiCol_Button, inactiveWindowColor );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImGui::GetColorU32( ImGuiCol_TitleBgCollapsed ) );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImGui::GetColorU32( ImGuiCol_TitleBg ) );
   ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 0.0f ) );
   ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, tabFramePadding );

   // Draw tab bar background color
   ImDrawList* dl = ImGui::GetWindowDrawList();
   const ImVec2 startDrawPos = ImGui::GetCursorPos();
   ImGui::SetCursorPos( ImVec2( startDrawPos.x, startDrawPos.y - 1.0f ) );
   dl->AddRectFilled(
       startDrawPos,
       ImVec2( startDrawPos.x + windowSize.x, startDrawPos.y + tabHeight - 1.0f ),
       inactiveWindowColor );

   // Draw all non selected tabs, whilst keeping the position of the selected one
   ImVec2 defaultTabSize( tabWidth, tabHeight );
   for ( int i = 0; i < profCount; ++i )
   {
      if( *selectedTab == i )
      {
         // Draw the selected tab as its own entity
         continue;
      }

      ImGui::PushID( i );
      const char* profName = displayableProfilerName( viewer.getProfiler( i ) );
      if ( ImGui::Button( profName, defaultTabSize ) )
      {
         *selectedTab = i;
      }
      ImGui::SetItemAllowOverlap(); // Since we will be drawing a close button on top this is needed
      ImGui::PopID();
      ImGui::SameLine();
   }

   // Draw the selected tab
   if ( *selectedTab >= 0 )
   {
      ImVec2 selectedTabPos = startDrawPos;
      selectedTabPos.x += (*selectedTab) * tabWidth + (*selectedTab) * tabFramePadding;
      ImGui::SetCursorPos( selectedTabPos );
      ImGui::PushStyleColor( ImGuiCol_Button, activeWindowColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, activeWindowColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, activeWindowColor );

      ImGui::Button( displayableProfilerName( viewer.getProfiler( *selectedTab ) ), defaultTabSize );
      ImGui::SetItemAllowOverlap(); // Since we will be drawing a close button on top this is needed
      ImGui::PopStyleColor( 3 );
   }

   // Draw the "+" button to add a tab
   ImVec2 addTabPos = startDrawPos;
   addTabPos.x += profCount * tabWidth + profCount * tabFramePadding;
   ImGui::SetCursorPos( addTabPos );
   ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
   if ( ImGui::Button( " + ", ImVec2( addTabWidth, tabHeight - 1.0f ) ) )
   {
      *selectedTab = profCount;
      viewer.addNewProfiler( nullptr, false );
   }
   if( ImGui::IsItemHovered() )
   {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted( "Add a new profiler" );
      ImGui::EndTooltip();
   }

   ImGui::PopStyleVar( 3 );
   ImGui::PopStyleColor( 4 );

   // Draw the "x" to close tabs
   ImVec2 closeButtonPos = startDrawPos;
   closeButtonPos.x += tabWidth - 25.0f;
   closeButtonPos.y += 5.0f;
   for ( int i = 0; i < profCount; ++i )
   {
      ImGui::PushID( i + 40 );
      ImGui::SetCursorPos( closeButtonPos );
      if( ImGui::Button( "x", ImVec2( 20.0f, 20.0f ) ) )
      {
         if( i == *selectedTab )
         {
            *selectedTab = std::min( profCount-2, *selectedTab );
         }
         else if( i < *selectedTab )
         {
            *selectedTab = std::max( *selectedTab-1, 0 );
         }

         viewer.removeProfiler( i );
      }
      closeButtonPos.x += tabWidth;
      ImGui::PopID();
   }

   ImGui::SetCursorPos( ImVec2( startDrawPos.x, startDrawPos.y + tabHeight + 10.0f ) );
}

namespace hop
{
Viewer::Viewer( uint32_t screenSizeX, uint32_t /*screenSizeY*/ )
    : _selectedTab( -1 ), _lastFrameTime( ClockType::now() ), _vsyncEnabled( hop::g_options.vsyncOn )
{
   hop::setupLODResolution( screenSizeX );
   renderer::createResources();
}

void Viewer::addNewProfiler( const char* processName, bool startRecording )
{
   _profilers.emplace_back( new hop::Profiler( processName ) );
   _profilers.back()->setRecording( startRecording );
}

void Viewer::removeProfiler( int index )
{
   assert( index >= 0 && index < (int)_profilers.size() );
   _profilers.erase( _profilers.begin() + index );
}

int Viewer::profilerCount() const
{
   return _profilers.size();
}

Profiler* Viewer::getProfiler( int index )
{
   assert( index >= 0 && index < (int)_profilers.size() );
   return _profilers[index].get();
}

void Viewer::fetchClientsData()
{
   for ( auto& p : _profilers )
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
   if ( _vsyncEnabled != hop::g_options.vsyncOn )
   {
      _vsyncEnabled = hop::g_options.vsyncOn;
      renderer::setVSync( _vsyncEnabled );
   }

   // Start the frame
   ImGui::NewFrame();
}

void Viewer::draw( uint32_t windowWidth, uint32_t windowHeight )
{
   renderer::setViewport( 0, 0, windowWidth, windowHeight );
   renderer::clearColorBuffer();

   ImGui::SetNextWindowPos( ImVec2( 0.0f, 0.0f ), ImGuiCond_Always );
   ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize, ImGuiCond_Always );
   ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
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

   const float dtTimeMs = ImGui::GetIO().DeltaTime * 1000;
   for ( auto& p : _profilers )
   {
      p->update( dtTimeMs );
   }

   drawTabs( *this, &_selectedTab );

   if ( _selectedTab >= 0 )
   {
      ImVec2 curPos = ImGui::GetCursorPos();
      _profilers[_selectedTab]->draw( curPos.x + 5.0f, curPos.y, windowWidth - 5.0f, windowHeight );
   }

   ImGui::PopStyleVar( 2 );
   ImGui::End();  // Window
   ImGui::PopStyleVar( 2 );

   // Render modal window, if any
   if ( modalWindowShowing() )
   {
      renderModalWindow();
   }

   handleHotkey();
   drawFloatingWindows();

   // Create the draw commands
   ImGui::Render();

   // Do the actual render
   renderer::renderDrawlist( ImGui::GetDrawData() );
}

bool Viewer::handleHotkey()
{
   if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Escape ) ) && !modalWindowShowing() )
   {
      hop::displayModalWindow( "Exit ?", hop::MODAL_TYPE_YES_NO, [&]() { g_run = false; } );
      return true;
   }

   return false;
}

Viewer::~Viewer() {}

}  // namespace hop