#include "ProfilerView.h"
#include "TimelineTracksView.h"
#include "Hop.h"
#include "common/TimelineTrack.h"
#include "common/Utils.h"
#include "common/TraceData.h"
#include "imgui/imgui.h"

#include "Lod.h"
#include "ModalWindow.h"
#include "Options.h"
#include "RendererGL.h"
#include <SDL_keycode.h>

#include <cassert>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>

extern bool g_run;

// Drawing constants
static constexpr float MAX_TRACE_HEIGHT = 50.0f;
static constexpr float MIN_TRACE_HEIGHT = 15.0f;
static constexpr uint32_t DISABLED_COLOR = 0xFF505050;
static constexpr uint32_t CORE_LABEL_COLOR = 0xFF333333;
static constexpr uint32_t CORE_LABEL_BORDER_COLOR = 0xFFAAAAAA;
static constexpr uint32_t SEPARATOR_HANDLE_COLOR = 0xFFAAAAAA;

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

static int closestLodLevel( hop::TimeDuration timelineDuration )
{
   int lodLvl = 0;
   while( lodLvl < hop::LOD_COUNT - 1 && timelineDuration > hop::LOD_CYCLES[lodLvl] )
   {
      ++lodLvl;
   }
   return lodLvl;
}

hop::ProfilerView::ProfilerView( hop::Profiler::SourceType type, int processId, const char* str )
   : _profiler( type, processId, str ), _lodLevel( 0 ), _highlightValue( 0.0f )
{
}

void hop::ProfilerView::fetchClientData()
{
   _profiler.fetchClientData();

   // Update the draw information according to new data
   const std::vector<TimelineTrack>& tlTrackData = _profiler.timelineTracks();
   const int newTrackCount = tlTrackData.size() - _trackViews.tracks.size();
   if( newTrackCount )
   {
      _trackViews.tracks.insert( _trackViews.tracks.end(), newTrackCount, TrackViewData{} );
   }
}

void hop::ProfilerView::update( float /*deltaTimeMs*/, float globalTimeMs, TimeDuration timelineDuration )
{
   HOP_PROF_FUNC();
   _highlightValue = (std::sin( 0.007f * globalTimeMs ) * 0.8f + 1.0f) / 2.0f;

   // Update current lod level
   _lodLevel = closestLodLevel( timelineDuration );

   updateTimelineTracks( _trackViews, _profiler );

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

void hop::ProfilerView::draw( float drawPosX, float drawPosY, const TimelineInfo& tlInfo, TimelineMsgArray* msgArray )
{
   HOP_PROF_FUNC();
   ImGui::SetCursorPos( ImVec2( drawPosX, drawPosY ) );

   if ( _trackViews.tracks.size() == 0 && !data().recording() )
   {
      displayBackgroundHelpMsg( ImGui::GetWindowWidth(), ImGui::GetWindowHeight() );
   }
   else
   {
      TimelineTrackDrawData drawData = { _profiler, tlInfo, _lodLevel, _highlightValue };
      hop::drawTimelineTracks( _trackViews, drawData, msgArray );
   }
}

bool hop::ProfilerView::handleHotkey()
{
   bool handled = false;
   // Let the tracks handle the hotkeys first.
   if( handleTimelineTracksHotKey( _trackViews ) )
   {
      return true;
   }

   // Otherwise, let the profiler handle it
   if ( ImGui::IsKeyDown( ImGui::GetKeyIndex( ImGuiKey_Delete ) ) && _trackViews.tracks.size() > 0 )
   {
      if ( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) &&
           !hop::modalWindowShowing() )
      {
         hop::displayModalWindow(
             "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&]() { clear(); } );
         handled = true;
      }
   }

   return handled;
}

bool hop::ProfilerView::handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel )
{
    bool handled = false;
   if ( _trackViews.draggedTrack > 0 )
   {
      // Find the previous track that is visible
      //int i = _draggedTrack - 1;
      // while ( i > 0 && _tracks[i].empty() )
      // {
      //    --i;
      // }

      //const float trackHeight = ( posY - _trackViews[i].localDrawPos[1] - THREAD_LABEL_HEIGHT );
      //_trackViews[i].trackHeight = trackHeight;

      handled = true;
   }

   return handled;
}

void hop::ProfilerView::clear()
{
   _profiler.clear();
   _trackViews.tracks.clear();
}

float hop::ProfilerView::canvasHeight() const
{
   return 9999.0f;
}

int hop::ProfilerView::lodLevel() const
{
   return _lodLevel;
}

const hop::Profiler& hop::ProfilerView::data() const
{
   return _profiler;
}

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