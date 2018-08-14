#include "Profiler.h"
#include "imgui/imgui.h"
#include "Lod.h"
#include "Stats.h"
#include "Utils.h"
#include "TraceDetail.h"
#include "TraceData.h"
#include "ModalWindow.h"
#include "miniz.h"
#include "Options.h"
#include "RendererGL.h"
#include "Utils.h"
#include <SDL_keycode.h>

#include <cassert>
#include <cmath>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <stdio.h>
#include <fstream>

extern bool g_run;

namespace
{
static std::chrono::time_point<std::chrono::system_clock> g_Time = std::chrono::system_clock::now();
static float g_deltaTimeMs = 0.0f;
std::vector<hop::Profiler*> _profilers;

namespace
{
   const uint32_t MAGIC_NUMBER = 1095780676; // "DIPA"
   struct SaveFileHeader
   {
      uint32_t magicNumber;
      uint32_t version;
      size_t uncompressedSize;
      uint32_t strDbSize;
      uint32_t threadCount;
   };

   void displayBackgroundHelpMsg()
   {
      const char* helpTxt =
          "-------------- Hop --------------\n\n"
          "Press 'R' to start/stop recording\n"
          "Right mouse click to get traces details\n"
          "Double click on a trace to focus it\n"
          "Right mouse drag to zoom on a region\n"
          "Right click on the timeline to create a bookmark\n"
          "Use arrow keys <-/-> to navigate bookmarks\n"
          "Use CTRL+F to search traces\n"
          "Use Del to delete traces\n";
      const auto pos = ImGui::GetWindowPos();
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const float windowHeightPxl = ImGui::GetWindowHeight();
      ImDrawList* DrawList = ImGui::GetWindowDrawList();
      auto size = ImGui::CalcTextSize( helpTxt );
      DrawList->AddText(
          ImGui::GetIO().Fonts->Fonts[0],
          30.0f,
          ImVec2( pos.x + windowWidthPxl / 2 - ( size.x ), pos.y + windowHeightPxl / 2 - size.y ),
          ImGui::GetColorU32( ImGuiCol_TextDisabled ),
          helpTxt );
   }
}

} // end of anonymous namespace

namespace hop
{

void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mousewheel )
{
   if ( !renderer::g_FontTexture ) renderer::createResources();

   ImGuiIO& io = ImGui::GetIO();

   // Setup display size (every frame to accommodate for window resizing)
   io.DisplaySize = ImVec2( (float)width, (float)height );
   // io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ?
   // ((float)display_h / h) : 0);

   // Setup time step
   auto curTime = std::chrono::system_clock::now();
   const float deltaTime = static_cast<float>(
       std::chrono::duration_cast<std::chrono::milliseconds>( ( curTime - g_Time ) ).count() );
   g_deltaTimeMs = deltaTime;
   io.DeltaTime = deltaTime * 0.001f; // ImGui expect seconds
   g_Time = curTime;

   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
   io.MousePos = ImVec2( (float)mouseX, (float)mouseY );

   io.MouseDown[0] = lmbPressed;
   io.MouseDown[1] = rmbPressed;
   io.MouseWheel = mousewheel;

   // Start the frame
   ImGui::NewFrame();
}

void draw( uint32_t windowWidth, uint32_t windowHeight )
{
   for( auto p : _profilers )
   {
      p->update( g_deltaTimeMs );
   }

   for ( auto p : _profilers )
   {
      p->draw( windowWidth, windowHeight );
   }

   if( g_options.debugWindow )
      hop::drawStatsWindow( g_stats );

   ImGui::Render();
}

void init()
{
   ImGuiIO& io = ImGui::GetIO();

   // Init keys ?
   // Init callbacks
   io.RenderDrawListsFn = renderer::renderDrawlist;  // Alternatively you can set this to NULL and call
                                                     // ImGui::GetDrawData() after ImGui::Render() to get the
                                                     // same ImDrawData pointer.
}

void addNewProfiler( Profiler* profiler )
{
   _profilers.push_back( profiler );
}

Profiler::Profiler( const char* name ) : _name( name )
{
   _server.start( name );
}

void Profiler::addTraces( const TraceData& traces, uint32_t threadIndex )
{
   // Ignore empty traces
   if( traces.ends.empty() ) return;

   // Add new thread as they come
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   // Update the current time
   if ( traces.ends.back() > _timeline.globalEndTime() )
      _timeline.setGlobalEndTime( traces.ends.back() );

   // If this is the first traces received from the thread, update the
   // start time as it may be earlier.
   if ( _tracks[threadIndex]._traces.ends.empty() )
   {
      // Find the earliest trace
      TimeStamp earliestTime = traces.ends[0] - traces.deltas[0];
      for ( size_t i = 1; i < traces.ends.size(); ++i )
      {
         earliestTime = std::min( earliestTime, traces.ends[i] - traces.deltas[i] );
      }
      // Set the timeline absolute start time to this new value
      const auto startTime = _timeline.globalStartTime();
      if ( startTime == 0 || earliestTime < startTime )
         _timeline.setGlobalStartTime( earliestTime );
   }

   _tracks[threadIndex].addTraces( traces );

   size_t totalTracesCount = 0;
   for( size_t i = 0; i < _tracks.size(); ++i )
   {
      totalTracesCount += _tracks[i]._traces.ends.size();
   }
   g_stats.traceCount = totalTracesCount;
}

void Profiler::fetchClientData()
{
   HOP_PROF_FUNC();

   _server.getPendingData(_serverPendingData);

   if( _recording )
   {
      HOP_PROF_SPLIT( "Fetching Str Data" );
      for( size_t i = 0; i <_serverPendingData.stringData.size(); ++i )
      {
         addStringData( _serverPendingData.stringData[i] );
      }
      HOP_PROF_SPLIT( "Fetching Traces" );
      for( size_t i = 0; i <_serverPendingData.traces.size(); ++i )
      {
         addTraces(_serverPendingData.traces[i], _serverPendingData.tracesThreadIndex[i] );
      }
      HOP_PROF_SPLIT( "Fetching Lock Waits" );
      for( size_t i = 0; i < _serverPendingData.lockWaits.size(); ++i )
      {
         addLockWaits(_serverPendingData.lockWaits[i], _serverPendingData.lockWaitThreadIndex[i] );
      }
      HOP_PROF_SPLIT( "Fetching Unlock Events" );
      for (size_t i = 0; i < _serverPendingData.unlockEvents.size(); ++i)
      {
         addUnlockEvents(_serverPendingData.unlockEvents[i], _serverPendingData.unlockEventsThreadIndex[i]);
      }
   }

   // We need to get the thread name even when not recording as they are only sent once
   for( size_t i = 0; i < _serverPendingData.threadNames.size(); ++i )
   {
      addThreadName( _serverPendingData.threadNames[i].second, _serverPendingData.threadNames[i].first );
   }
}

void Profiler::addStringData( const std::vector<char>& strData )
{
   HOP_PROF_FUNC();
   // We should read the string data even when not recording since the string data
   // is sent only once (the first time a function is used)
   if ( !strData.empty() )
   {
      _strDb.addStringData( strData );
   }
}

void Profiler::addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if( !lockWaits.ends.empty() )
   {
      _tracks[threadIndex].addLockWaits( lockWaits );
   }
}

void Profiler::addUnlockEvents( const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if( !unlockEvents.empty() )
   {
      _tracks[threadIndex].addUnlockEvents( unlockEvents );
   }
}

void Profiler::addThreadName( TStrPtr_t name, uint32_t threadIndex )
{
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   assert( name != 0 ); // should not be empty name

   _tracks[threadIndex].setTrackName( name );
}

Profiler::~Profiler()
{
   _server.stop();
}

} // end of namespace hop


// static bool drawDispTrace( const hop::DisplayableTraceFrame& frame, size_t& i )
// {
//    const auto& trace = frame.traces[i];
//    if( !trace.flags ) return false;

//    bool isOpen = false;
//    // bool isOpen = trace.classNameIndex
//    //                   ? ImGui::TreeNode( trace.classNameIndex, "%s::%s :    %f us", 0trace.name, trace.deltaTime )
//    //                   : ImGui::TreeNode( trace.fctNameIndex, "%s :    %f us", trace.name, trace.deltaTime );

//    if( isOpen )
//    {
//       ++i;
//       while( frame.traces[i].flags )
//       {
//          drawDispTrace( frame, i );
//       }
//       ImGui::TreePop();
//       ++i;
//    }
//    else
//    {
//       int lvl = 0;
//       for ( size_t j = i + 1; j < frame.traces.size(); ++j )
//       {
//          if ( frame.traces[j].flags )
//          {
//             ++lvl;
//          }
//          else
//          {
//             --lvl;
//             if ( lvl < 0 )
//             {
//                i = std::min( ++j, frame.traces.size()-1);
//                break;
//             }
//          }
//       }
//       assert( i < frame.traces.size() );
//    }
//    return isOpen;
// }

void hop::Profiler::update( float deltaTimeMs ) noexcept
{
   _timeline.update( deltaTimeMs );
   _tracks.update( deltaTimeMs, _timeline.duration() );
}

static constexpr float TOOLBAR_BUTTON_HEIGHT = 15.0f;
static constexpr float TOOLBAR_BUTTON_WIDTH = 15.0f;
static constexpr float TOOLBAR_BUTTON_PADDING = 5.0f;

static bool drawPlayStopButton( const ImVec2& drawPos, bool isRecording )
{
   HOP_PROF_FUNC();
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const auto& mousePos = ImGui::GetMousePos();
   const bool hovering =
       ImGui::IsMouseHoveringWindow() &&
       hop::ptInRect(
           mousePos.x, mousePos.y,
           drawPos.x, drawPos.y,
           drawPos.x + TOOLBAR_BUTTON_WIDTH,
           drawPos.y + TOOLBAR_BUTTON_HEIGHT );

   if ( isRecording )
   {
      DrawList->AddRectFilled(
          drawPos,
          ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + TOOLBAR_BUTTON_HEIGHT ),
          hovering ? ImColor( 0.9f, 0.0f, 0.0f ) : ImColor( 0.7f, 0.0f, .0f ) );\
      if( hovering )
      {
         ImGui::BeginTooltip();
         ImGui::Text("Stop recording traces ('r')");
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

      if( hovering )
      {
         ImGui::BeginTooltip();
         ImGui::Text("Start recording traces ('r')");
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
   const bool hovering =
       ImGui::IsMouseHoveringWindow() &&
       hop::ptInRect(
           mousePos.x, mousePos.y,
           drawPos.x, drawPos.y,
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

   if( active && hovering )
   {
      ImGui::BeginTooltip();
      ImGui::Text("Delete all recorded traces ('Del')");
      ImGui::EndTooltip();
   }

   return hovering && active && ImGui::IsMouseClicked( 0 );
}

static void drawStatusIcon( const ImVec2& drawPos, hop::SharedMemory::ConnectionState state )
{
   ImColor col( 0.5f, 0.5f, 0.5f );
   const char* msg = nullptr;
   switch ( state )
   {
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
   if( hovering && msg )
   {
      ImGui::BeginTooltip();
      ImGui::Text( "%s", msg );
      ImGui::EndTooltip();
   }
}

void hop::Profiler::draw( uint32_t /*windowWidth*/, uint32_t /*windowHeight*/ )
{
   HOP_PROF_FUNC();
   ImGui::SetNextWindowPos( ImVec2( 0.0f, 0.0f ), ImGuiCond_Always );
   ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize, ImGuiCond_Always );
   ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );

   ImGui::Begin(
       _name.c_str(),
       nullptr,
       ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar |
           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar |
           ImGuiWindowFlags_NoResize );

   // Reset the style var so the floating windows can be drawn properly
   ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 0, 0 ) );
   ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 5.0f );

   drawMenuBar();
   // Render modal window, if any
   renderModalWindow();
   drawOptionsWindow( g_options );

   const auto toolbarDrawPos = ImGui::GetCursorScreenPos();
   if( drawPlayStopButton( toolbarDrawPos, _recording ) )
   {
      setRecording( !_recording );
   }

   auto deleteTracePos = toolbarDrawPos;
   deleteTracePos.x += (2.0f*TOOLBAR_BUTTON_PADDING) + TOOLBAR_BUTTON_WIDTH;
   if( drawDeleteTracesButton( deleteTracePos, _tracks.size() > 0 ) )
   {
      hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&](){ clear(); } );
   }

   auto statusPos = toolbarDrawPos;
   statusPos.x += ImGui::GetWindowWidth() - 25.0f;
   statusPos.y += 5.0f;
   drawStatusIcon( statusPos, _server.connectionState() );

   if( _tracks.size() == 0 && !_recording )
   {
      displayBackgroundHelpMsg();
   }
   else
   {
      //  Move timeline to the most recent trace if Live mode is on
      if ( _recording && _timeline.realtime() )
      {
         _timeline.moveToPresentTime( Timeline::ANIMATION_TYPE_NONE );
      }

      _timeline.draw( _tracks.totalHeight() );

      // Push clip rect for canvas and draw
      ImGui::PushClipRect(
          ImVec2( _timeline.canvasPosX(), _timeline.canvasPosY() ), ImVec2( 99999, 99999 ), true );
      ImGui::BeginChild( "TimelineCanvas" );

      auto timelineActions =
          _tracks.draw( TimelineTracks::DrawInfo{_timeline.constructTimelineInfo(), _strDb} );

      ImGui::EndChild();
      ImGui::PopClipRect();
      // Handle deferred timeline actions created by the module
      _timeline.handleDeferredActions( timelineActions );
   }

   handleHotkey();
   handleMouse();

   ImGui::PopStyleVar(2);
   ImGui::End();
   ImGui::PopStyleVar();
}

void hop::Profiler::drawMenuBar()
{
   HOP_PROF_FUNC();
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
            g_options.optionWindowOpened = true;
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
         setRecording( false );
         saveToFile( path );
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
         openFile( path );
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
      ImGui::Text("Hop version %.1f\n\nThis is a help menu\nPretty useful isnt it?\n\n", HOP_VERSION);
      if ( ImGui::Button( "Yes indeed", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
   }
}

void hop::Profiler::handleHotkey()
{
   // Let the tracks handle the hotkeys first.
   if( _tracks.handleHotkey() )
      return;

   if( ImGui::IsKeyReleased( SDL_SCANCODE_HOME ) )
   {
      _timeline.moveToStart();
   }
   else if( ImGui::IsKeyReleased( SDL_SCANCODE_END ) )
   {
      _timeline.moveToPresentTime();
      _timeline.setRealtime ( true );
   }
   else if( ImGui::IsKeyReleased( 'r' ) && ImGui::IsRootWindowOrAnyChildFocused() )
   {
      setRecording( !_recording );
   }
   else if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'z' ) )
   {
      _timeline.undoNavigation();
   }
   else if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'y' ) )
   {
      _timeline.redoNavigation();
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_LEFT ) )
   {
      _timeline.previousBookmark();
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_RIGHT ) )
   {
      _timeline.nextBookmark();
   }
   else if( ImGui::IsKeyDown( SDLK_DELETE ) && _tracks.size() > 0 )
   {
      if( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && !hop::modalWindowShowing() )
         hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&](){ clear(); } );
   }
}

void hop::Profiler::handleMouse()
{
   const auto mousePos = ImGui::GetMousePos();
   const bool lmb = ImGui::IsMouseDown( 0 );
   const bool rmb = ImGui::IsMouseDown( 1 );
   const float wheel = ImGui::GetIO().MouseWheel;
   bool mouseHandled = _tracks.handleMouse( mousePos.x, mousePos.y, lmb, rmb, wheel );
   if ( !mouseHandled )
   {
      _timeline.handleMouse( mousePos.x, mousePos.y, lmb, rmb, wheel );
   }
}

void hop::Profiler::setRecording(bool recording)
{
   _recording = recording;
   _server.setRecording(recording);
   if (recording)
   {
      _timeline.setRealtime(true);
   }
}

bool hop::Profiler::saveToFile( const char* path )
{
   displayModalWindow( "Saving...", MODAL_TYPE_NO_CLOSE );
   std::thread t( [this, path]() {
      // Compute the size of the serialized data
      const size_t dbSerializedSize = serializedSize( _strDb );
      const size_t timelineSerializedSize = serializedSize( _timeline );
      std::vector<size_t> timelineTrackSerializedSize( _tracks.size() );
      for ( size_t i = 0; i < _tracks.size(); ++i )
      {
         timelineTrackSerializedSize[i] = serializedSize( _tracks[i] );
      }

      const size_t totalSerializedSize =
          std::accumulate( timelineTrackSerializedSize.begin(), timelineTrackSerializedSize.end(), size_t{0} ) +
          timelineSerializedSize +
          dbSerializedSize;

      std::vector<char> data( totalSerializedSize );

      size_t index = serialize( _strDb, &data[0] );
      index += serialize( _timeline, &data[index] );
      for ( size_t i = 0; i < _tracks.size(); ++i )
      {
         index += serialize( _tracks[i], &data[index] );
      }

      mz_ulong compressedSize = compressBound( totalSerializedSize );
      std::vector<char> compressedData( compressedSize );
      int compressionStatus = compress(
          (unsigned char*)compressedData.data(),
          &compressedSize,
          (const unsigned char*)&data[0],
          totalSerializedSize );
      if ( compressionStatus != Z_OK )
      {
         closeModalWindow();
         displayModalWindow( "Compression failed. File not saved!", MODAL_TYPE_CLOSE );
         return false;
      }

      std::ofstream of( path, std::ofstream::binary );
      SaveFileHeader header = {MAGIC_NUMBER,
                               1,
                               totalSerializedSize,
                               (uint32_t)dbSerializedSize,
                               (uint32_t)_tracks.size()};
      of.write( (const char*)&header, sizeof( header ) );
      of.write( &compressedData[0], totalSerializedSize );

      closeModalWindow();
      return true;
   } );

   t.detach();

   return true;
}

bool hop::Profiler::openFile( const char* path )
{
   std::ifstream input( path, std::ifstream::binary );
   if ( input.is_open() )
   {
      clear();

      displayModalWindow( "Loading...", MODAL_TYPE_NO_CLOSE );
      std::thread t( [this, path]() {
         std::ifstream input( path, std::ifstream::binary );
         std::vector<char> data(
             ( std::istreambuf_iterator<char>( input ) ), ( std::istreambuf_iterator<char>() ) );

         SaveFileHeader* header = (SaveFileHeader*)&data[0];

         if( header->magicNumber != MAGIC_NUMBER )
         {
            closeModalWindow();
            displayModalWindow( "Not a valid hop file.", MODAL_TYPE_CLOSE );
            return false;
         }

         std::vector<char> uncompressedData( header->uncompressedSize );
         mz_ulong uncompressedSize = uncompressedData.size();

         int uncompressStatus = uncompress(
             (unsigned char*)uncompressedData.data(),
             &uncompressedSize,
             (unsigned char*)&data[sizeof( SaveFileHeader )],
             data.size() - sizeof( SaveFileHeader ) );

         if ( uncompressStatus != Z_OK )
         {
            closeModalWindow();
            displayModalWindow( "Error uncompressing file. Nothing will be loaded", MODAL_TYPE_CLOSE );
            return false;
         }

         size_t i = 0;
         const size_t dbSize = deserialize( &uncompressedData[i], _strDb );
         assert( dbSize == header->strDbSize );
         i += dbSize;

         const size_t timelineSize = deserialize( &uncompressedData[i], _timeline );
         i += timelineSize;

         std::vector<TimelineTrack> timelineTracks( header->threadCount );
         for ( uint32_t j = 0; j < header->threadCount; ++j )
         {
            size_t timelineTrackSize = deserialize( &uncompressedData[i], timelineTracks[j] );
            addTraces( timelineTracks[j]._traces, j );
            addLockWaits( timelineTracks[j]._lockWaits, j );
            i += timelineTrackSize;
         }
         closeModalWindow();
         return true;
      } );

      t.detach();

      return true;
   }
   displayModalWindow( "File not found", MODAL_TYPE_CLOSE );
   return false;
}

void hop::Profiler::clear()
{
   _server.clear();
   _strDb.clear();
   _tracks.clear();
   _timeline.setGlobalStartTime( 0 );
   _timeline.clearBookmarks();
   _recording = false;
   g_stats.traceCount = 0;
}

