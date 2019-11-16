#include "ProfilerView.h"
#include "Viewer.h"

#include "Cursor.h"
#include "Lod.h"
#include "ModalWindow.h"
#include "Options.h"
#include "RendererGL.h"
#include "Stats.h"

#include "common/Profiler.h"
#include "common/Utils.h"

#include "platform/Platform.h"

#include "imgui/imgui.h"

extern bool g_run;

static constexpr float MAX_FULL_SIZE_TAB_COUNT = 8.0f;
static constexpr float TAB_HEIGHT = 30.0f;
static constexpr float TOOLBAR_BUTTON_HEIGHT = 15.0f;
static constexpr float TOOLBAR_BUTTON_WIDTH = 15.0f;
static constexpr float TOOLBAR_BUTTON_PADDING = 5.0f;

static void addNewProfilerPopUp( hop::Viewer* v, hop::Profiler::SourceType type )
{
   hop::displayStringInputModalWindow( "Enter name or PID of process", [=]( const char* str ) {
      if ( type == hop::Profiler::SRC_TYPE_PROCESS )
      {
         v->addNewProfiler( str, false );
      }
      else if ( type == hop::Profiler::SRC_TYPE_FILE )
      {
         v->openProfilerFile( str );
      }
   } );
}

static void drawMenuBar( hop::Viewer* v )
{
   static const char* const menuAddProfiler = "Add Profiler";
   static const char* const menuSaveAsHop = "Save as...";
   static const char* const menuOpenHopFile = "Open";
   static const char* const menuHelp = "Help";
   const char* menuAction = NULL;

   if ( ImGui::BeginMenuBar() )
   {
      if ( ImGui::BeginMenu( "Menu" ) )
      {
         const int profIdx = v->activeProfilerIndex();
         if ( ImGui::MenuItem( menuAddProfiler, NULL ) )
         {
            addNewProfilerPopUp( v, hop::Profiler::SRC_TYPE_PROCESS );
         }
         if( ImGui::MenuItem( menuSaveAsHop, NULL, false, profIdx >= 0 ) )
         {
            // hop::Profiler* prof = v->getProfiler( profIdx );
            // hop::displayStringInputModalWindow(
            //     menuSaveAsHop, [=]( const char* path ) { prof->saveToFile( path ); } );
         }
         if( ImGui::MenuItem( menuOpenHopFile, NULL ) )
         {
            hop::displayStringInputModalWindow( menuOpenHopFile, [=]( const char* path ) {
               v->openProfilerFile( path );
            } );
         }
         if( ImGui::MenuItem( menuHelp, NULL ) )
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

   if ( menuAction == menuHelp )
   {
      ImGui::OpenPopup( menuAction );
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

static bool drawPlayStopButton( const ImVec2& drawPos, bool isRecording )
{
   HOP_PROF_FUNC();
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const auto& mousePos = ImGui::GetMousePos();
   const bool hovering = ImGui::IsMouseHoveringWindow() && hop::ptInRect(
                                                               mousePos.x,
                                                               mousePos.y,
                                                               drawPos.x,
                                                               drawPos.y,
                                                               drawPos.x + TOOLBAR_BUTTON_WIDTH,
                                                               drawPos.y + TOOLBAR_BUTTON_HEIGHT );

   if ( isRecording )
   {
      DrawList->AddRectFilled(
          drawPos,
          ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + TOOLBAR_BUTTON_HEIGHT ),
          hovering ? ImColor( 0.9f, 0.0f, 0.0f ) : ImColor( 0.7f, 0.0f, .0f ) );
      if ( hovering )
      {
         ImGui::BeginTooltip();
         ImGui::Text( "Stop recording traces ('r')" );
         ImGui::EndTooltip();
      }
   }
   else
   {
      ImVec2 pts[] = {
          drawPos,
          ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + ( TOOLBAR_BUTTON_HEIGHT * 0.5 ) ),
          ImVec2( drawPos.x, drawPos.y + TOOLBAR_BUTTON_WIDTH )};
      DrawList->AddConvexPolyFilled(
          pts, 3, hovering ? ImColor( 0.0f, 0.9f, 0.0f ) : ImColor( 0.0f, 0.7f, 0.0f ) );

      if ( hovering )
      {
         ImGui::BeginTooltip();
         ImGui::Text( "Start recording traces ('r')" );
         ImGui::EndTooltip();
      }
   }

   ImGui::SetCursorScreenPos(
       ImVec2( drawPos.x, drawPos.y + TOOLBAR_BUTTON_HEIGHT + TOOLBAR_BUTTON_PADDING ) );

   return hovering && ImGui::IsMouseClicked( 0 );
}

static bool drawDeleteTracesButton( const ImVec2& drawPos, bool active )
{
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const auto& mousePos = ImGui::GetMousePos();
   const bool hovering = ImGui::IsMouseHoveringWindow() && hop::ptInRect(
                                                               mousePos.x,
                                                               mousePos.y,
                                                               drawPos.x,
                                                               drawPos.y,
                                                               drawPos.x + TOOLBAR_BUTTON_WIDTH,
                                                               drawPos.y + TOOLBAR_BUTTON_HEIGHT );

   ImColor col = active ? ( hovering ? ImColor( 0.9f, 0.0f, 0.0f ) : ImColor( 0.7f, 0.0f, 0.0f ) )
                        : ImColor( 0.5f, 0.5f, 0.5f );

   DrawList->AddLine(
       drawPos,
       ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + TOOLBAR_BUTTON_HEIGHT ),
       col,
       3.0f );
   DrawList->AddLine(
       ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y ),
       ImVec2( drawPos.x, drawPos.y + TOOLBAR_BUTTON_HEIGHT ),
       col,
       3.0f );

   if ( active && hovering )
   {
      ImGui::BeginTooltip();
      ImGui::Text( "Delete all recorded traces ('Del')" );
      ImGui::EndTooltip();
   }

   return hovering && active && ImGui::IsMouseClicked( 0 );
}

static void drawStatusIcon( const ImVec2 drawPos, hop::SharedMemory::ConnectionState state )
{
   ImColor col( 0.5f, 0.5f, 0.5f );
   const char* msg = nullptr;
   switch ( state )
   {
      case hop::SharedMemory::NO_TARGET_PROCESS:
         col = ImColor( 0.6f, 0.6f, 0.6f );
         msg = "No target process";
         break;
      case hop::SharedMemory::NOT_CONNECTED:
         col = ImColor( 0.8f, 0.0f, 0.0f );
         msg = "No shared memory found";
         break;
      case hop::SharedMemory::CONNECTED:
         col = ImColor( 0.0f, 0.8f, 0.0f );
         msg = "Connected";
         break;
      case hop::SharedMemory::CONNECTED_NO_CLIENT:
         col = ImColor( 0.8f, 0.8f, 0.0f );
         msg = "Connected to shared memory, but no client";
         break;
      case hop::SharedMemory::PERMISSION_DENIED:
         col = ImColor( 0.6f, 0.2f, 0.0f );
         msg = "Permission to shared memory or semaphore denied";
         break;
      case hop::SharedMemory::UNKNOWN_CONNECTION_ERROR:
         col = ImColor( 0.4f, 0.0f, 0.0f );
         msg = "Unknown connection error";
         break;
   }

   ImDrawList* DrawList = ImGui::GetWindowDrawList();
   DrawList->AddCircleFilled( drawPos, 10.0f, col );

   const auto& mousePos = ImGui::GetMousePos();
   const bool hovering = ImGui::IsMouseHoveringWindow() &&
                         hop::ptInCircle( mousePos.x, mousePos.y, drawPos.x, drawPos.y, 10.0f );
   if ( hovering && msg )
   {
      ImGui::BeginTooltip();
      ImGui::Text( "%s", msg );
      ImGui::EndTooltip();
   }
}

static void drawToolbar( const ImVec2 toolbarDrawPos, float canvasWidth, hop::ProfilerView* profView )
{
   if( profView )
   {
      const bool isRecording = profView->data().recording();
      if ( profView->data().sourceType() == hop::Profiler::SRC_TYPE_PROCESS &&
         drawPlayStopButton( toolbarDrawPos, isRecording ) )
      {
         profView->setRecording( !isRecording );
      }

      auto deleteTracePos = toolbarDrawPos;
      deleteTracePos.x += ( 2.0f * TOOLBAR_BUTTON_PADDING ) + TOOLBAR_BUTTON_WIDTH;
      if ( drawDeleteTracesButton( deleteTracePos, profView->canvasHeight() > 0 ) )
      {
         hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { profView->clear(); } );
      }

      auto statusPos = toolbarDrawPos;
      statusPos.x += canvasWidth - 25.0f;
      statusPos.y += 5.0f;
      drawStatusIcon( statusPos, profView->data().connectionState() );
   }

   ImGui::SetCursorPosY( toolbarDrawPos.y + TOOLBAR_BUTTON_HEIGHT );
}

static int displayableProfilerName( const hop::ProfilerView* prof, char* outName, uint32_t size )
{
   int pid;
   const char* profName = prof->data().nameAndPID( &pid );
   if( !profName )
   {
      return snprintf( outName, size, "%s", "<No Target Process>" );
   }

   return snprintf( outName, size, "%s (%d)", profName, pid );
}

static bool drawAddTabButton( const ImVec2& drawPos )
{
   const float addTabWidth = TAB_HEIGHT * 1.0;
   bool clicked = false;
   ImGui::SetCursorPos( drawPos );
   ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
   if ( ImGui::Button( " + ", ImVec2( addTabWidth, TAB_HEIGHT - 1.0f ) ) )
   {
      clicked = true;
   }
   if ( ImGui::IsItemHovered() )
   {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted( "Add a new profiler" );
      ImGui::EndTooltip();
   }
   ImGui::PopStyleVar( 1 );

   return clicked;
}

static int drawTabs( const ImVec2 drawPos, hop::Viewer& viewer, int selectedTab )
{
   const ImVec2 windowSize = ImGui::GetWindowSize();

   const float tabBarWidth = windowSize.x - TAB_HEIGHT;
   const float fullSizeTab = tabBarWidth / MAX_FULL_SIZE_TAB_COUNT;
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
   ImGui::SetCursorPos( ImVec2( drawPos.x, drawPos.y - 1.0f ) );
   dl->AddRectFilled(
       drawPos,
       ImVec2( drawPos.x + windowSize.x, drawPos.y + TAB_HEIGHT - 1.0f ),
       inactiveWindowColor );

   int newTabSelection = selectedTab;

   // Draw all non selected tabs, whilst keeping the position of the selected one
   ImVec2 defaultTabSize( tabWidth, TAB_HEIGHT );
   for ( int i = 0; i < profCount; ++i )
   {
      if ( selectedTab == i )
      {
         // Draw the selected tab as its own entity
         ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX() + tabWidth, drawPos.y ) );
         continue;
      }

      char profName[64];
      displayableProfilerName( viewer.getProfiler( i ), profName, sizeof( profName ) );
      ImGui::PushID( i );
      if ( ImGui::Button( profName, defaultTabSize ) )
      {
         newTabSelection = i;
      }
      ImGui::SetItemAllowOverlap();  // Since we will be drawing a close button on top this is
                                     // needed
      if ( ImGui::IsItemHovered() )
      {
         ImGui::BeginTooltip();
         ImGui::TextUnformatted( profName );
         ImGui::EndTooltip();
      }

      ImGui::PopID();
      ImGui::SameLine();
   }

   // Draw the selected tab
   if ( selectedTab >= 0 )
   {
      ImVec2 selectedTabPos = drawPos;
      selectedTabPos.x += ( selectedTab ) * tabWidth + ( selectedTab ) * tabFramePadding;
      ImGui::SetCursorPos( selectedTabPos );
      ImGui::PushStyleColor( ImGuiCol_Button, activeWindowColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, activeWindowColor );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, activeWindowColor );

      char profName[64];
      displayableProfilerName( viewer.getProfiler( selectedTab ), profName, sizeof( profName ) );
      ImGui::Button( profName, defaultTabSize );
      ImGui::SetItemAllowOverlap();  // Since we will be drawing a close button on top this is
                                     // needed
      if ( ImGui::IsItemHovered() )
      {
         ImGui::BeginTooltip();
         ImGui::TextUnformatted( profName );
         ImGui::EndTooltip();
      }

      ImGui::PopStyleColor( 3 );
   }

   // Draw the "+" button to add a tab
   ImVec2 addTabPos = drawPos;
   addTabPos.x += profCount * tabWidth + profCount * tabFramePadding;
   if ( drawAddTabButton( addTabPos ) )
   {
      addNewProfilerPopUp( &viewer, hop::Profiler::SRC_TYPE_PROCESS );
   }

   ImGui::PopStyleVar( 2 );
   ImGui::PopStyleColor( 4 );

   // Draw the "x" to close tabs
   ImGui::PushStyleColor( ImGuiCol_Button, 0XFF101077 );
   ImGui::PushStyleColor( ImGuiCol_ButtonHovered, 0XFF101099 );
   ImGui::PushStyleColor( ImGuiCol_ButtonActive, 0XFF101055 );
   ImVec2 closeButtonPos = drawPos;
   closeButtonPos.x += tabWidth - 25.0f;
   closeButtonPos.y += 5.0f;
   for ( int i = 0; i < profCount; ++i )
   {
      ImGui::PushID( i + 40 );
      ImGui::SetCursorPos( closeButtonPos );
      if ( ImGui::Button( "x", ImVec2( 20.0f, 20.0f ) ) )
      {
         newTabSelection = viewer.removeProfiler( i );
      }
      closeButtonPos.x += tabWidth;
      ImGui::PopID();
   }
   ImGui::PopStyleColor( 3 );

   // Restore cursor pos for next drawing
   ImGui::SetCursorPos( ImVec2( drawPos.x, drawPos.y + TAB_HEIGHT + 10.0f ) );

   return newTabSelection;
}

static void updateOptions( const hop::ProfilerView* selectedProf, hop::Stats& stats )
{
   const hop::Profiler& profData = selectedProf->data();
   const hop::ProfilerStats profStats = profData.stats();
   stats.currentLOD = selectedProf->lodLevel();
   stats.stringDbSize = profStats.strDbSize;
   stats.traceCount = profStats.traceCount;
   stats.clientSharedMemSize = profStats.clientSharedMemSize;
}

static void updateProfilers(
    hop::TimeDuration tlDuration,
    std::vector<std::unique_ptr<hop::ProfilerView> >& profilers,
    int selectedTab )
{
   const float dtTimeMs = ImGui::GetIO().DeltaTime * 1000;
   const float globalTimeMs = ImGui::GetTime() * 1000;
   for ( auto& p : profilers )
   {
      p->update( dtTimeMs, globalTimeMs, tlDuration );
   }

   if( hop::g_options.debugWindow && selectedTab >= 0 )
   {
      updateOptions( profilers[ selectedTab ].get(), hop::g_stats );
   }
}

static bool profilerAlreadyExist(
    const std::vector<std::unique_ptr<hop::ProfilerView> >& profilers,
    int pid,
    const char* processName )
{
   bool alreadyExist = false;
   if( pid > -1 )
   {
      alreadyExist =
          std::find_if(
              profilers.begin(), profilers.end(), [pid]( const std::unique_ptr<hop::ProfilerView>& pv ) {
                 int curPid;
                 pv->data().nameAndPID( &curPid );
                 return curPid == pid;
              } ) != profilers.end();
   }
   else  // Do a string comparison since we do not have a pid
   {
      alreadyExist = std::find_if(
                         profilers.begin(),
                         profilers.end(),
                         [processName]( const std::unique_ptr<hop::ProfilerView>& pv ) {
                            return strcmp( pv->data().nameAndPID(), processName ) == 0;
                         } ) != profilers.end();
   }

   return alreadyExist;
}

namespace hop
{
Viewer::Viewer( uint32_t screenSizeX, uint32_t /*screenSizeY*/ )
    : _selectedTab( -1 ),
      _lastFrameTime( ClockType::now() ),
      _vsyncEnabled( hop::g_options.vsyncOn )
{
   hop::setupLODResolution( screenSizeX );
   hop::initCursors();
   renderer::createResources();
}

int Viewer::addNewProfiler( const char* processName, bool startRecording )
{
   const int pid = getPIDFromString( processName );
   if( profilerAlreadyExist( _profilers, pid, processName ) )
   {
      hop::displayModalWindow( "Cannot profile process twice !", hop::MODAL_TYPE_ERROR );
      return -1;
   }

   const hop::ProcessInfo procInfo = pid != -1 ? hop::getProcessInfoFromPID( pid )
                                               : hop::getProcessInfoFromProcessName( processName );

   _profilers.emplace_back( new hop::ProfilerView( Profiler::SRC_TYPE_PROCESS, procInfo.pid, processName ) );
   _profilers.back()->setRecording( startRecording );
   _selectedTab = _profilers.size() - 1;
   return _selectedTab;
}

void Viewer::openProfilerFile( const char* /*filePath*/ )
{
   assert( !_pendingProfilerLoad.valid() );

   //std::string strPath = filePath;
   // _pendingProfilerLoad = std::async(
   //     std::launch::async,
   //     []( std::string path ) {
   //        Profiler* prof = new hop::Profiler();
   //        prof->setSource( Profiler::SRC_TYPE_FILE, -1, path.c_str() );
   //        return prof;
   //     },
   //     strPath );
}

int Viewer::removeProfiler( int index )
{
   assert( index >= 0 && index < (int)_profilers.size() );

   if ( index == _selectedTab )
   {
      _selectedTab = std::min( profilerCount() - 2, _selectedTab );
   }
   else if ( index < _selectedTab )
   {
      _selectedTab = std::max( _selectedTab - 1, 0 );
   }
   _profilers.erase( _profilers.begin() + index );
   return _selectedTab;
}

int Viewer::profilerCount() const { return _profilers.size(); }

int Viewer::activeProfilerIndex() const { return _selectedTab; }

const ProfilerView* Viewer::getProfiler( int index ) const
{
   assert( index >= 0 && index < (int)_profilers.size() );
   return _profilers[index].get();
}

void Viewer::fetchClientsData()
{
   // if ( _pendingProfilerLoad.valid() )
   // {
   //    const auto waitRes = _pendingProfilerLoad.wait_for( std::chrono::microseconds( 200 ) );
   //    if ( waitRes == std::future_status::ready )
   //    {
   //       std::unique_ptr<Profiler> loadedProf( _pendingProfilerLoad.get() );
   //       if ( strlen( loadedProf->nameAndPID() ) > 0 )
   //       {
   //          _profilers.emplace_back( std::move(loadedProf) );
   //          _selectedTab = _profilers.size() - 1;
   //       }
   //    }
   // }
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

   // Reset cursor at start of the frame
   hop::setCursor( hop::CURSOR_ARROW );
}

void Viewer::draw( uint32_t windowWidth, uint32_t windowHeight )
{
   const auto drawStart = std::chrono::system_clock::now();

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

   // We only want to remove the padding for the viewer window and not all of its childrens.
   ImGui::PopStyleVar( 2 );

   // Update
   updateProfilers( _timeline.duration(), _profilers, _selectedTab );

   // Then draw
   drawMenuBar( this );
   _selectedTab = drawTabs( ImGui::GetCursorPos(), *this, _selectedTab );
   ProfilerView* const selectedProf = _selectedTab >= 0 ? _profilers[_selectedTab].get() : nullptr;
   drawToolbar( ImGui::GetCursorPos(), ImGui::GetWindowWidth(), selectedProf );

   _timeline.draw();
   if ( selectedProf )
   {
      const ImVec2 curPos = ImGui::GetCursorPos();
      _timeline.beginDrawCanvas( selectedProf->canvasHeight() );
      selectedProf->draw( curPos.x, curPos.y, windowWidth - 5.0f, windowHeight );
      _timeline.endDrawCanvas();
   }
   
   handleHotkey();
   handleMouse( selectedProf );

   ImGui::End();  // Hop Viewer Window

   // Render modal window, if any
   if ( modalWindowShowing() )
   {
      renderModalWindow();
   }

   drawOptionsWindow( hop::g_options );

   // Update draw stats before drawing the actual stats window
   const auto drawEnd = std::chrono::system_clock::now();
   hop::g_stats.drawingTimeMs =
       std::chrono::duration<double, std::milli>( ( drawEnd - drawStart ) ).count();

   // Draw stats window
   if ( hop::g_options.debugWindow )
   {
      hop::drawStatsWindow( hop::g_stats );
   }

   // Create the draw commands
   ImGui::Render();

   // Do the actual render
   renderer::renderDrawlist( ImGui::GetDrawData() );

   hop::drawCursor();
}

bool Viewer::handleHotkey()
{
   if( !modalWindowShowing() )
   {
      if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Escape ) ) )
      {
         hop::displayModalWindow( "Exit ?", hop::MODAL_TYPE_YES_NO, [&]() { g_run = false; } );
         return true;
      }
      else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'w' ) )
      {
         if ( _selectedTab >= 0 )
         {
            removeProfiler( _selectedTab );
         }
      }
      else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 't' ) )
      {
         addNewProfilerPopUp( this, Profiler::SRC_TYPE_PROCESS );
      }
   }

   return false;
}

bool Viewer::handleMouse( ProfilerView* selectedProf )
{
   const auto mousePos = ImGui::GetMousePos();
   const bool lmb = ImGui::IsMouseDown( 0 );
   const bool rmb = ImGui::IsMouseDown( 1 );
   const float wheel = ImGui::GetIO().MouseWheel;

   bool mouseHandled = false;
   if ( selectedProf )
   {
      mouseHandled = selectedProf->handleMouse( mousePos.x, mousePos.y, lmb, rmb, wheel );
   }

   if( !mouseHandled )
   {
      _timeline.handleMouse( mousePos.x, mousePos.y, lmb, rmb, wheel );
   }

   return mouseHandled;
}

Viewer::~Viewer()
{
   hop::uninitCursors();
}

}  // namespace hop