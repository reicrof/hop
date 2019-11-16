#include "ProfilerView.h"
#include "Hop.h"
#include "common/TimelineTrack.h"
#include "common/Utils.h"
#include "common/TraceData.h"

#include "Lod.h"
//#include "TraceDetail.h"
#include "ModalWindow.h"
#include "Options.h"
#include "RendererGL.h"
#include <SDL_keycode.h>

#include "imgui/imgui.h"

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>

extern bool g_run;

// Drawing constants
static constexpr float THREAD_LABEL_HEIGHT = 20.0f;
static constexpr float MAX_TRACE_HEIGHT = 50.0f;
static constexpr float MIN_TRACE_HEIGHT = 15.0f;
static constexpr uint32_t DISABLED_COLOR = 0xFF505050;
static constexpr uint32_t CORE_LABEL_COLOR = 0xFF333333;
static constexpr uint32_t CORE_LABEL_BORDER_COLOR = 0xFFAAAAAA;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR = 0xFFAAAAAA;
static const char* CTXT_MENU_STR = "Context Menu";

// Static variable mutable from options
static float TRACE_HEIGHT = 20.0f;
static float TRACE_VERTICAL_PADDING = 2.0f;
static float PADDED_TRACE_SIZE = TRACE_HEIGHT + TRACE_VERTICAL_PADDING;

static void displayBackgroundHelpMsg( uint32_t windowWidth, uint32_t windowHeight )
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

static float computeCanvasSize( const std::vector<hop::TrackDrawInfo>& tdi )
{
   float tracksHeight = 0.0f;
   if( const size_t trackCount = tdi.size() )
   {
      tracksHeight = tdi[trackCount-1].absoluteDrawPos[1] + tdi[trackCount-1].trackHeight;
      tracksHeight -= ( ImGui::GetWindowHeight() - tdi[0].absoluteDrawPos[1] );
   }

   return tracksHeight;
}

hop::ProfilerView::ProfilerView( hop::Profiler::SourceType type, int processId, const char* str )
   : _profiler( type, processId, str ), _lodLevel( 0 ), _draggedTrack( -1 ), _highlightValue( 0.0f )
{

}

void hop::ProfilerView::update( float /*deltaTimeMs*/, float globalTimeMs, TimeDuration timelineDuration )
{
   _highlightValue = (std::sin( 0.007f * globalTimeMs ) * 0.8f + 1.0f) / 2.0f;

   // Update current lod level
   int lodLvl = 0;
   while ( lodLvl < LOD_COUNT - 1 && timelineDuration > LOD_NANOS[lodLvl] )
   {
      ++lodLvl;
   }
   _lodLevel = lodLvl;

   //.update( deltaTimeMs );
   // _tracks.update( globalTimeMs, /*_timeline.duration()*/ );
   // if( _name.empty() || _pid < 0 )
   // {
   //    const char* name = _server.processInfo( &_pid );
   //    if( name )
   //    {
   //       _name = name;
   //    }
   // }
}

void hop::ProfilerView::setRecording( bool recording )
{
   _profiler.setRecording( recording );
}

void hop::ProfilerView::draw( float drawPosX, float drawPosY, float canvasWidth, float canvasHeight )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   ImGui::BeginChild( "Timeline" );
   if ( _trackDrawInfos.size() == 0 && !data().recording() )
   {
      displayBackgroundHelpMsg( canvasWidth, canvasHeight );
   }
   else
   {
      //  Move timeline to the most recent trace if Live mode is on
      // if ( _recording && _timeline.realtime() )
      // {
      //    _timeline.moveToPresentTime( Timeline::ANIMATION_TYPE_NONE );
      // }

      // Draw the timeline ruler
      //_timeline.draw();

      // Start the canvas drawing
      //_timeline.beginDrawCanvas( computeCanvasSize( _tracks ) );

      // Draw the tracks inside the canvaws
      // auto timelineActions =
      //     _tracks.draw( /*TimelineTracksDrawInfo{_timeline.constructTimelineInfo()*/, _strDb} );

      //_timeline.drawOverlay();

      //_timeline.endDrawCanvas();

      // Handle deferred timeline actions created by the module
      //_timeline.handleDeferredActions( timelineActions );
   }

   ImGui::EndChild();  //"Timeline"
}

void hop::ProfilerView::handleHotkey()
{
   // Let the tracks handle the hotkeys first.
   //if ( _tracks.handleHotkey() ) return;

   // if ( ImGui::IsKeyReleased( ImGui::GetKeyIndex( ImGuiKey_Home ) ) )
   // {
   //    _timeline.moveToStart();
   // }
   // else if ( ImGui::IsKeyReleased( ImGui::GetKeyIndex( ImGuiKey_End ) ) )
   // {
   //    _timeline.moveToPresentTime();
   //    _timeline.setRealtime( true );
   // }
   // else if ( ImGui::IsKeyReleased( 'r' ) && ImGui::IsRootWindowOrAnyChildFocused() )
   // {
   //    setRecording( !_recording );
   // }
   // else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'z' ) )
   // {
   //    _timeline.undoNavigation();
   // }
   // else if ( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'y' ) )
   // {
   //    _timeline.redoNavigation();
   // }
   // else if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_LeftArrow ) ) )
   // {
   //    _timeline.previousBookmark();
   // }
   // else if ( ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_RightArrow ) ) )
   // {
   //    _timeline.nextBookmark();
   // }
   //else if ( ImGui::IsKeyDown( ImGui::GetKeyIndex( ImGuiKey_Delete ) ) && _trackDrawInfos.size() > 0 )
   if ( ImGui::IsKeyDown( ImGui::GetKeyIndex( ImGuiKey_Delete ) ) && _trackDrawInfos.size() > 0 )
   {
      if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) &&
           !hop::modalWindowShowing() )
         hop::displayModalWindow(
             "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
   }
}

bool hop::ProfilerView::handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel )
{
    bool handled = false;
   if ( _draggedTrack > 0 )
   {
      // Find the previous track that is visible
      int i = _draggedTrack - 1;
      // while ( i > 0 && _tracks[i].empty() )
      // {
      //    --i;
      // }

      const float trackHeight = ( posY - _trackDrawInfos[i].localDrawPos[1] - THREAD_LABEL_HEIGHT );
      _trackDrawInfos[i].trackHeight = trackHeight;

      handled = true;
   }

   return handled;
}

void hop::ProfilerView::clear()
{
   _profiler.clear();
   _trackDrawInfos.clear();
}

const float hop::ProfilerView::canvasHeight() const
{
   return 9999.0f;
}

const int hop::ProfilerView::lodLevel() const
{
   return _lodLevel;
}

const hop::Profiler& hop::ProfilerView::data() const
{
   return _profiler;
}

// void hop::Profiler::setRecording( bool recording )
// {
//    _recording = recording;
//    _server.setRecording( recording );
//    if ( recording )
//    {
//       _timeline.setRealtime( true );
//    }
// }

// bool hop::Profiler::saveToFile( const char* savePath )
// {
//    displayModalWindow( "Saving...", MODAL_TYPE_NO_CLOSE );
//    setRecording( false );
//    std::string path( savePath );
//    std::thread t( [this, path]() {
//       // Compute the size of the serialized data
//       const size_t dbSerializedSize = serializedSize( _strDb );
//       const size_t timelineSerializedSize = serializedSize( _timeline );
//       std::vector<size_t> timelineTrackSerializedSize( _tracks.size() );
//       for ( size_t i = 0; i < _tracks.size(); ++i )
//       {
//          timelineTrackSerializedSize[i] = serializedSize( _tracks[i] );
//       }

//       const size_t totalSerializedSize =
//           std::accumulate(
//               timelineTrackSerializedSize.begin(), timelineTrackSerializedSize.end(), size_t{0} ) +
//           timelineSerializedSize + dbSerializedSize;

//       std::vector<char> data( totalSerializedSize );

//       size_t index = serialize( _strDb, &data[0] );
//       index += serialize( _timeline, &data[index] );
//       for ( size_t i = 0; i < _tracks.size(); ++i )
//       {
//          index += serialize( _tracks[i], &data[index] );
//       }

//       mz_ulong compressedSize = compressBound( totalSerializedSize );
//       std::vector<char> compressedData( compressedSize );
//       int compressionStatus = compress(
//           (unsigned char*)compressedData.data(),
//           &compressedSize,
//           (const unsigned char*)&data[0],
//           totalSerializedSize );
//       if ( compressionStatus != Z_OK )
//       {
//          closeModalWindow();
//          displayModalWindow( "Compression failed. File not saved!", MODAL_TYPE_ERROR );
//          return false;
//       }

//       std::ofstream of( path, std::ofstream::binary );
//       SaveFileHeader header = {MAGIC_NUMBER,
//                                1,
//                                totalSerializedSize,
//                                (uint32_t)dbSerializedSize,
//                                (uint32_t)_tracks.size()};
//       of.write( (const char*)&header, sizeof( header ) );
//       of.write( &compressedData[0], compressedSize );

//       closeModalWindow();
//       return true;
//    } );

//    t.detach();

//    return true;
// }

// bool hop::Profiler::openFile( const char* path )
// {
//    std::ifstream input( path, std::ifstream::binary );
//    if ( input.is_open() )
//    {
//       clear();

//       displayModalWindow( "Loading...", MODAL_TYPE_NO_CLOSE );
//       std::vector<char> data(
//           ( std::istreambuf_iterator<char>( input ) ), ( std::istreambuf_iterator<char>() ) );

//       SaveFileHeader* header = (SaveFileHeader*)&data[0];

//       if ( header->magicNumber != MAGIC_NUMBER )
//       {
//          closeModalWindow();
//          displayModalWindow( "Not a valid hop file.", MODAL_TYPE_ERROR );
//          return false;
//       }

//       std::vector<char> uncompressedData( header->uncompressedSize );
//       mz_ulong uncompressedSize = uncompressedData.size();

//       int uncompressStatus = uncompress(
//           (unsigned char*)uncompressedData.data(),
//           &uncompressedSize,
//           (unsigned char*)&data[sizeof( SaveFileHeader )],
//           data.size() - sizeof( SaveFileHeader ) );

//       if ( uncompressStatus != Z_OK )
//       {
//          closeModalWindow();
//          displayModalWindow( "Error uncompressing file. Nothing will be loaded", MODAL_TYPE_ERROR );
//          return false;
//       }

//       size_t i = 0;
//       const size_t dbSize = deserialize( &uncompressedData[i], _strDb );
//       assert( dbSize == header->strDbSize );
//       i += dbSize;

//       //const size_t timelineSize = deserialize( &uncompressedData[i], _timeline );
//       //i += timelineSize;

//       std::vector<TimelineTrack> timelineTracks( header->threadCount );
//       for ( uint32_t j = 0; j < header->threadCount; ++j )
//       {
//          size_t timelineTrackSize = deserialize( &uncompressedData[i], timelineTracks[j] );
//          addTraces( timelineTracks[j]._traces, j );
//          addLockWaits( timelineTracks[j]._lockWaits, j );
//          i += timelineTrackSize;
//       }
//       _srcType = SRC_TYPE_FILE;
//       closeModalWindow();
//       return true;
//    }
//    displayModalWindow( "File not found", MODAL_TYPE_ERROR );
//    return false;
// }