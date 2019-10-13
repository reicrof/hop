#include "Profiler.h"
#include "imgui/imgui.h"
#include "Lod.h"
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
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <stdio.h>
#include <fstream>

extern bool g_run;

namespace
{
const uint32_t MAGIC_NUMBER = 1095780676;  // "DIPA"
struct SaveFileHeader
{
   uint32_t magicNumber;
   uint32_t version;
   size_t uncompressedSize;
   uint32_t strDbSize;
   uint32_t threadCount;
};

void displayBackgroundHelpMsg( uint32_t windowWidth, uint32_t windowHeight )
{
   const char* helpTxt =
       "-------------- Hop --------------\n\n"
       "Press 'R' to start/stop recording\n"
       "Right mouse click to get traces details\n"
       "Double click on a trace to focus it\n"
       "Right mouse drag to zoom on a region\n"
       "Left mouse drag to measure time in region\n"
       "Right click on the timeline to create a bookmark\n"
       "Use arrow keys <-/-> to navigate bookmarks\n"
       "Use CTRL+F to search traces\n"
       "Use Del to delete traces\n";
   const auto pos = ImGui::GetWindowPos();
   ImDrawList* DrawList = ImGui::GetWindowDrawList();
   auto size = ImGui::CalcTextSize( helpTxt );
   DrawList->AddText(
       ImGui::GetIO().Fonts->Fonts[0],
       30.0f,
       ImVec2( pos.x + windowWidth / 2 - ( size.x ), pos.y + windowHeight / 2 - size.y ),
       ImGui::GetColorU32( ImGuiCol_TextDisabled ),
       helpTxt );
}

float computeCanvasSize( const hop::TimelineTracks& tracks )
{
   float tracksHeight = tracks.totalHeight();
   if ( tracksHeight > 0.0f )
   {
      tracksHeight -= ( ImGui::GetWindowHeight() - tracks[0]._absoluteDrawPos[1] );
   }
   return tracksHeight;
}
}  // end of anonymous namespace

namespace hop
{
Profiler::Profiler() : _srcType( SourceType::NONE ), _viewType( ViewType::PROFILER )
{
}

const char* Profiler::nameAndPID( int* processId )
{
   if( processId ) *processId = _pid;
   return _name.c_str();
}

ProfilerStats Profiler::stats() const
{
   ProfilerStats stats = {};
   stats.lodLevel = _tracks.lodLevel();
   stats.strDbSize = _strDb.sizeInBytes();
   stats.clientSharedMemSize = _server.sharedMemorySize();
   for ( size_t i = 0; i < _tracks.size(); ++i )
   {
      stats.traceCount += _tracks[i]._traces.entries.ends.size();
   }

   return stats;
}

bool Profiler::setSource( SourceType type, int processId, const char* str )
{
   switch( type )
   {
      case SourceType::PROCESS:
        return setProcess( processId, str );
      case SourceType::FILE:
        return openFile( str );
      case SourceType::NONE:
        assert(false);
        return false;
   }

   return false;
}

Profiler::SourceType Profiler::sourceType() const { return _srcType; }

void Profiler::addTraces( const TraceData& traces, uint32_t threadIndex )
{
   // Ignore empty traces
   if ( traces.entries.ends.empty() ) return;

   // Add new thread as they come
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   // Update the current time
   if ( traces.entries.ends.back() > _timeline.globalEndTime() )
      _timeline.setGlobalEndTime( traces.entries.ends.back() );

   // If this is the first traces received from the thread, update the
   // start time as it may be earlier.
   if ( _tracks[threadIndex]._traces.entries.ends.empty() )
   {
      // Find the earliest trace
      TimeStamp earliestTime = traces.entries.ends[0] - traces.entries.deltas[0];
      for ( size_t i = 1; i < traces.entries.ends.size(); ++i )
      {
         earliestTime = std::min( earliestTime, traces.entries.ends[i] - traces.entries.deltas[i] );
      }
      // Set the timeline absolute start time to this new value
      const auto startTime = _timeline.globalStartTime();
      if ( startTime == 0 || earliestTime < startTime )
         _timeline.setGlobalStartTime( earliestTime );
   }

   _tracks[threadIndex].addTraces( traces );
}

void Profiler::fetchClientData()
{
   HOP_PROF_FUNC();

   _server.getPendingData( _serverPendingData );

   if ( _recording )
   {
      HOP_PROF_SPLIT( "Fetching Str Data" );

      addStringData( _serverPendingData.stringData );

      HOP_PROF_SPLIT( "Fetching Traces" );
      for( const auto& threadTraces : _serverPendingData.tracesPerThread )
      {
         addTraces( threadTraces.second, threadTraces.first );
      }
      HOP_PROF_SPLIT( "Fetching Lock Waits" );
      for( const auto& lockwaits : _serverPendingData.lockWaitsPerThread )
      {
         addLockWaits( lockwaits.second, lockwaits.first );
      }
      HOP_PROF_SPLIT( "Fetching Unlock Events" );
      for( const auto& unlockEvents : _serverPendingData.unlockEventsPerThread )
      {
         addUnlockEvents( unlockEvents.second, unlockEvents.first );
      }
      HOP_PROF_SPLIT( "Fetching CoreEvents" );
      for( const auto& coreEvents : _serverPendingData.coreEventsPerThread )
      {
         addCoreEvents( coreEvents.second, coreEvents.first );
      }
      HOP_PROF_SPLIT( "Fetching CoreEvents" );
      for( const auto& statEvents : _serverPendingData.statEventsPerThread )
      {
         addStatEvents( statEvents.second, statEvents.first );
      }
   }

   // We need to get the thread name even when not recording as they are only sent once
   for ( size_t i = 0; i < _serverPendingData.threadNames.size(); ++i )
   {
      addThreadName(
          _serverPendingData.threadNames[i].second, _serverPendingData.threadNames[i].first );
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

   if ( !lockWaits.entries.ends.empty() )
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

   if ( !unlockEvents.empty() )
   {
      _tracks[threadIndex].addUnlockEvents( unlockEvents );
   }
}

void Profiler::addCoreEvents( const std::vector<CoreEvent>& coreEvents, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   if ( !coreEvents.empty() )
   {
      _tracks[threadIndex].addCoreEvents( coreEvents );
   }
}

void Profiler::addStatEvents( const std::vector<StatEvent>& statEvents, uint32_t /*threadIndex*/ )
{
   HOP_PROF_FUNC();
   if ( !statEvents.empty() )
   {
      _stats.addStatEvents( statEvents );
   }
}

void Profiler::addThreadName( StrPtr_t name, uint32_t threadIndex )
{
   // Check if new thread
   if ( threadIndex >= _tracks.size() )
   {
      _tracks.resize( threadIndex + 1 );
   }

   assert( name != 0 );  // should not be empty name

   _tracks[threadIndex].setTrackName( name );
}

Profiler::~Profiler() { _server.stop(); }

}  // end of namespace hop

// static bool drawDispTrace( const hop::DisplayableTraceFrame& frame, size_t& i )
// {
//    const auto& trace = frame.traces[i];
//    if( !trace.flags ) return false;

//    bool isOpen = false;
//    // bool isOpen = trace.classNameIndex
//    //                   ? ImGui::TreeNode( trace.classNameIndex, "%s::%s :    %f us",
//    0trace.name, trace.deltaTime )
//    //                   : ImGui::TreeNode( trace.fctNameIndex, "%s :    %f us", trace.name,
//    trace.deltaTime );

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

void hop::Profiler::update( float deltaTimeMs, float globalTimeMs )
{
   _timeline.update( deltaTimeMs );
   _tracks.update( globalTimeMs, _timeline.duration() );
   if( _name.empty() || _pid < 0 )
   {
      const char* name = _server.processInfo( &_pid );
      if( name )
      {
         _name = name;
      }
   }
}

static constexpr float TOOLBAR_BUTTON_HEIGHT = 15.0f;
static constexpr float TOOLBAR_BUTTON_WIDTH = 15.0f;
static constexpr float TOOLBAR_BUTTON_PADDING = 5.0f;

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

   const ImColor col =
       active ? ( hovering ? ImColor( 0.9f, 0.0f, 0.0f ) : ImColor( 0.7f, 0.0f, 0.0f ) )
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

static bool drawViewTypeButton( const ImVec2& drawPos, hop::Profiler::ViewType viewType )
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

   const ImColor col = hovering ? ImColor( 1.0f, 1.0f, 1.0f ) : ImColor( 0.7f, 0.7f, 0.7f );

   const char* tooltip = "";
   if( viewType == hop::Profiler::ViewType::PROFILER )
   {
      tooltip               = "Change To Stats View";
      const float lineWidth = 5.0f;
      ImVec2 bottomPt( drawPos.x, drawPos.y + TOOLBAR_BUTTON_HEIGHT );
      ImVec2 topPt( drawPos.x, drawPos.y + 0.7f * TOOLBAR_BUTTON_HEIGHT );
      for( int i = 0; i < 3; ++i )
      {
         DrawList->AddLine( bottomPt, topPt, col, lineWidth );
         bottomPt.x += lineWidth + 2.0f;
         topPt.x += lineWidth + 2.0f;
         topPt.y -= 0.3f * TOOLBAR_BUTTON_HEIGHT;   
      }
   }
   else if( viewType == hop::Profiler::ViewType::STATS )
   {
      tooltip = "Change To Profiler View";
      const float lineWidth = 3.0f;
      float lineOffset      = 0;
      const ImVec2 topLine( drawPos.x, drawPos.y + lineOffset );
      const ImVec2 topLineEnd( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + lineOffset );
      lineOffset += lineWidth + 2.0f;
      const ImVec2 midLine( drawPos.x, drawPos.y + lineOffset );
      const ImVec2 midLineEnd( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + lineOffset );
      lineOffset += lineWidth + 2.0f;
      const ImVec2 lastLine1( drawPos.x, drawPos.y + lineOffset );
      const ImVec2 lastLineEnd1(
          drawPos.x + TOOLBAR_BUTTON_WIDTH * 0.5 - 2.0f, drawPos.y + lineOffset );
      const ImVec2 lastLine2( drawPos.x + TOOLBAR_BUTTON_WIDTH * 0.5, drawPos.y + lineOffset );
      const ImVec2 lastLineEnd2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + lineOffset );
      DrawList->AddLine( topLine, topLineEnd, col, lineWidth );
      DrawList->AddLine( midLine, midLineEnd, col, lineWidth );
      DrawList->AddLine( lastLine1, lastLineEnd1, col, lineWidth );
      DrawList->AddLine( lastLine2, lastLineEnd2, col, lineWidth );
   }

   if ( hovering )
   {
      ImGui::BeginTooltip();
      ImGui::Text( tooltip );
      ImGui::EndTooltip();
   }

   return hovering && ImGui::IsMouseClicked( 0 );
}

static void drawStatusIcon( const ImVec2& drawPos, hop::SharedMemory::ConnectionState state )
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

void hop::Profiler::draw( float drawPosX, float drawPosY, float canvasWidth, float canvasHeight )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   const ImVec2 toolbarDrawPos = ImVec2( drawPosX, drawPosY );
   if ( sourceType() == SourceType::PROCESS && drawPlayStopButton( toolbarDrawPos, _recording ) )
   {
      setRecording( !_recording );
   }

   ImVec2 deleteTracePos = toolbarDrawPos;
   deleteTracePos.x += ( 2.0f * TOOLBAR_BUTTON_PADDING ) + TOOLBAR_BUTTON_WIDTH;
   if ( drawDeleteTracesButton( deleteTracePos, _tracks.size() > 0 ) )
   {
      hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
   }

   ImVec2 viewTypeButton = deleteTracePos;
   viewTypeButton.x += ( 2.0f * TOOLBAR_BUTTON_PADDING ) + TOOLBAR_BUTTON_WIDTH;
   if( drawViewTypeButton( viewTypeButton, _viewType ) )
   {
      _viewType = _viewType == ViewType::PROFILER ? ViewType::STATS : ViewType::PROFILER;
   }

   auto statusPos = toolbarDrawPos;
   statusPos.x += canvasWidth - 25.0f;
   statusPos.y += 5.0f;
   drawStatusIcon( statusPos, _server.connectionState() );

   ImGui::BeginChild( "Timeline" );
   if ( _tracks.size() == 0 && !_recording )
   {
      displayBackgroundHelpMsg( canvasWidth, canvasHeight );
   }
   else
   {
      //  Move timeline to the most recent trace if Live mode is on
      if ( _recording && _timeline.realtime() )
      {
         _timeline.moveToPresentTime( Timeline::ANIMATION_TYPE_NONE );
      }

      // Draw the timeline ruler
      _timeline.draw();

      // Start the canvas drawing
      _timeline.beginDrawCanvas( computeCanvasSize( _tracks ) );

      // Draw the tracks inside the canvas
      std::vector< TimelineMessage > timelineActions;
      if( _viewType == ViewType::PROFILER )
      {
         timelineActions = _tracks.draw( TimelineTracksDrawInfo{_timeline.constructTimelineInfo(), _strDb} );
      }
      else
      {
         timelineActions = _stats.draw( _timeline.constructTimelineInfo(), _strDb );
      }

      _timeline.drawOverlay();
      _timeline.endDrawCanvas();

      // Handle deferred timeline actions created by the module
      _timeline.handleDeferredActions( timelineActions );
   }

   handleHotkey();
   handleMouse();

   ImGui::EndChild();  //"Timeline"
}

void hop::Profiler::handleHotkey()
{
   // Let the tracks handle the hotkeys first.
   if ( _tracks.handleHotkey() ) return;

   if ( ImGui::IsKeyReleased( ImGui::GetKeyIndex( ImGuiKey_Home ) ) )
   {
      _timeline.moveToStart();
   }
   else if ( ImGui::IsKeyReleased( ImGui::GetKeyIndex( ImGuiKey_End ) ) )
   {
      _timeline.moveToPresentTime();
      _timeline.setRealtime( true );
   }
   else if ( ImGui::IsKeyReleased( 'r' ) && ImGui::IsRootWindowOrAnyChildFocused() )
   {
      setRecording( !_recording );
   }
   else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'z' ) )
   {
      _timeline.undoNavigation();
   }
   else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'y' ) )
   {
      _timeline.redoNavigation();
   }
   else if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_LeftArrow ) ) )
   {
      _timeline.previousBookmark();
   }
   else if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_RightArrow ) ) )
   {
      _timeline.nextBookmark();
   }
   else if ( ImGui::IsKeyDown( ImGui::GetKeyIndex( ImGuiKey_Delete ) ) && _tracks.size() > 0 )
   {
      if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) &&
           !hop::modalWindowShowing() )
         hop::displayModalWindow(
             "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
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

void hop::Profiler::setRecording( bool recording )
{
   _recording = recording;
   _server.setRecording( recording );
   if ( recording )
   {
      _timeline.setRealtime( true );
   }
}

bool hop::Profiler::saveToFile( const char* savePath )
{
   displayModalWindow( "Saving...", MODAL_TYPE_NO_CLOSE );
   setRecording( false );
   std::string path( savePath );
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
          std::accumulate(
              timelineTrackSerializedSize.begin(), timelineTrackSerializedSize.end(), size_t{0} ) +
          timelineSerializedSize + dbSerializedSize;

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
         displayModalWindow( "Compression failed. File not saved!", MODAL_TYPE_ERROR );
         return false;
      }

      std::ofstream of( path, std::ofstream::binary );
      SaveFileHeader header = {MAGIC_NUMBER,
                               1,
                               totalSerializedSize,
                               (uint32_t)dbSerializedSize,
                               (uint32_t)_tracks.size()};
      of.write( (const char*)&header, sizeof( header ) );
      of.write( &compressedData[0], compressedSize );

      closeModalWindow();
      return true;
   } );

   t.detach();

   return true;
}

bool hop::Profiler::setProcess( int processId, const char* process )
{
   _server.stop();
   _srcType = SourceType::PROCESS;
   return _server.start( processId, process );
}

bool hop::Profiler::openFile( const char* path )
{
   std::ifstream input( path, std::ifstream::binary );
   if ( input.is_open() )
   {
      clear();

      displayModalWindow( "Loading...", MODAL_TYPE_NO_CLOSE );
      std::vector<char> data(
          ( std::istreambuf_iterator<char>( input ) ), ( std::istreambuf_iterator<char>() ) );

      SaveFileHeader* header = (SaveFileHeader*)&data[0];

      if ( header->magicNumber != MAGIC_NUMBER )
      {
         closeModalWindow();
         displayModalWindow( "Not a valid hop file.", MODAL_TYPE_ERROR );
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
         displayModalWindow( "Error uncompressing file. Nothing will be loaded", MODAL_TYPE_ERROR );
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
      _srcType = SourceType::FILE;
      closeModalWindow();
      return true;
   }
   displayModalWindow( "File not found", MODAL_TYPE_ERROR );
   return false;
}

void hop::Profiler::clear()
{
   _server.clear();
   _strDb.clear();
   _tracks.clear();
   _timeline.clear();
   _recording = false;
}
