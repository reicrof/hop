#include "Profiler.h"
#include "imgui/imgui.h"
#include "Lod.h"
#include "Stats.h"
#include "Utils.h"
#include "TraceDetail.h"
#include "DisplayableTraces.h"
#include "ModalWindow.h"
#include "miniz.h"
#include "Options.h"
#include <SDL_keycode.h>

// Todo : I dont like this dependency
#include "Server.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

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
static GLuint g_FontTexture = 0;
std::vector<hop::Profiler*> _profilers;

// This is the main rendering function that you have to implement and provide to ImGui (via setting
// up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or
// (0.375f,0.375f)
void renderDrawlist( ImDrawData* draw_data )
{
   // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates !=
   // framebuffer coordinates)
   ImGuiIO& io = ImGui::GetIO();
   int fb_width = (int)( io.DisplaySize.x * io.DisplayFramebufferScale.x );
   int fb_height = (int)( io.DisplaySize.y * io.DisplayFramebufferScale.y );
   if ( fb_width == 0 || fb_height == 0 ) return;
   draw_data->ScaleClipRects( io.DisplayFramebufferScale );

   // We are using the OpenGL fixed pipeline to make the example code simpler to read!
   // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor
   // enabled, vertex/texcoord/color pointers.
   GLint last_texture;
   glGetIntegerv( GL_TEXTURE_BINDING_2D, &last_texture );
   GLint last_viewport[4];
   glGetIntegerv( GL_VIEWPORT, last_viewport );
   GLint last_scissor_box[4];
   glGetIntegerv( GL_SCISSOR_BOX, last_scissor_box );
   glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT );
   glEnable( GL_BLEND );
   glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
   glDisable( GL_CULL_FACE );
   glDisable( GL_DEPTH_TEST );
   glEnable( GL_SCISSOR_TEST );
   glEnableClientState( GL_VERTEX_ARRAY );
   glEnableClientState( GL_TEXTURE_COORD_ARRAY );
   glEnableClientState( GL_COLOR_ARRAY );
   glEnable( GL_TEXTURE_2D );
   // glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context

   // Setup viewport, orthographic projection matrix
   glViewport( 0, 0, (GLsizei)fb_width, (GLsizei)fb_height );
   glMatrixMode( GL_PROJECTION );
   glPushMatrix();
   glLoadIdentity();
   glOrtho( 0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f );
   glMatrixMode( GL_MODELVIEW );
   glPushMatrix();
   glLoadIdentity();

// Render command lists
#define OFFSETOF( TYPE, ELEMENT ) ( ( size_t ) & ( ( (TYPE*)0 )->ELEMENT ) )
   for ( int n = 0; n < draw_data->CmdListsCount; n++ )
   {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
      const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
      glVertexPointer(
          2,
          GL_FLOAT,
          sizeof( ImDrawVert ),
          (const GLvoid*)( (const char*)vtx_buffer + OFFSETOF( ImDrawVert, pos ) ) );
      glTexCoordPointer(
          2,
          GL_FLOAT,
          sizeof( ImDrawVert ),
          (const GLvoid*)( (const char*)vtx_buffer + OFFSETOF( ImDrawVert, uv ) ) );
      glColorPointer(
          4,
          GL_UNSIGNED_BYTE,
          sizeof( ImDrawVert ),
          (const GLvoid*)( (const char*)vtx_buffer + OFFSETOF( ImDrawVert, col ) ) );

      for ( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
      {
         const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
         if ( pcmd->UserCallback )
         {
            pcmd->UserCallback( cmd_list, pcmd );
         }
         else
         {
            glBindTexture( GL_TEXTURE_2D, ( GLuint )(intptr_t)pcmd->TextureId );
            glScissor(
                (int)pcmd->ClipRect.x,
                (int)( fb_height - pcmd->ClipRect.w ),
                (int)( pcmd->ClipRect.z - pcmd->ClipRect.x ),
                (int)( pcmd->ClipRect.w - pcmd->ClipRect.y ) );
            glDrawElements(
                GL_TRIANGLES,
                (GLsizei)pcmd->ElemCount,
                sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                idx_buffer );
         }
         idx_buffer += pcmd->ElemCount;
      }
   }
#undef OFFSETOF

   // Restore modified state
   glDisableClientState( GL_COLOR_ARRAY );
   glDisableClientState( GL_TEXTURE_COORD_ARRAY );
   glDisableClientState( GL_VERTEX_ARRAY );
   glBindTexture( GL_TEXTURE_2D, (GLuint)last_texture );
   glMatrixMode( GL_MODELVIEW );
   glPopMatrix();
   glMatrixMode( GL_PROJECTION );
   glPopMatrix();
   glPopAttrib();
   glViewport(
       last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3] );
   glScissor(
       last_scissor_box[0],
       last_scissor_box[1],
       (GLsizei)last_scissor_box[2],
       (GLsizei)last_scissor_box[3] );
}

void createResources()
{
   // Build texture atlas
   ImGuiIO& io = ImGui::GetIO();
   unsigned char* pixels;
   int width, height;
   io.Fonts->GetTexDataAsRGBA32(
       &pixels, &width, &height );  // Load as RGBA 32-bits (75% of the memory is wasted, but
                                    // default font is so small) because it is more likely to be
                                    // compatible with user's existing shaders. If your ImTextureId
                                    // represent a higher-level concept than just a GL texture id,
                                    // consider calling GetTexDataAsAlpha8() instead to save on GPU
                                    // memory.

   // Upload texture to graphics system
   GLint last_texture;
   glGetIntegerv( GL_TEXTURE_BINDING_2D, &last_texture );
   glGenTextures( 1, &g_FontTexture );
   glBindTexture( GL_TEXTURE_2D, g_FontTexture );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );

   // Store our identifier
   io.Fonts->TexID = (void*)(intptr_t)g_FontTexture;

   // Restore state
   glBindTexture( GL_TEXTURE_2D, last_texture );
}

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
   if ( !g_FontTexture ) createResources();

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
   io.RenderDrawListsFn = renderDrawlist;  // Alternatively you can set this to NULL and call
                                           // ImGui::GetDrawData() after ImGui::Render() to get the
                                           // same ImDrawData pointer.
                                           // Clipboard stuff ?
}

void addNewProfiler( Profiler* profiler )
{
   _profilers.push_back( profiler );
}

Profiler::Profiler( const char* name ) : _name( name )
{
   _server.start( name, g_options.glFinishByDefault );
}

void Profiler::addTraces( const DisplayableTraces& traces, uint32_t threadIndex )
{
   // Ignore empty traces
   if( traces.ends.empty() ) return;

   // Add new thread as they come
   if ( threadIndex >= _tracesPerThread.size() )
   {
      _tracesPerThread.resize( threadIndex + 1 );
   }

   // Update the current time
   if ( traces.ends.back() > _timeline.absolutePresentTime() )
      _timeline.setAbsolutePresentTime( traces.ends.back() );

   // If this is the first traces received from the thread, update the
   // start time as it may be earlier.
   if ( _tracesPerThread[threadIndex]._traces.ends.empty() )
   {
      // Find the earliest trace
      TimeStamp earliestTime = traces.ends[0] - traces.deltas[0];
      for ( size_t i = 1; i < traces.ends.size(); ++i )
      {
         earliestTime = std::min( earliestTime, traces.ends[i] - traces.deltas[i] );
      }
      // Set the timeline absolute start time to this new value
      const auto startTime = _timeline.absoluteStartTime();
      if ( startTime == 0 || earliestTime < startTime )
         _timeline.setAbsoluteStartTime( earliestTime );
   }

   _tracesPerThread[threadIndex].addTraces( traces );

   size_t totalTracesCount = 0;
   for( const auto& t : _tracesPerThread )
   {
      totalTracesCount += t._traces.ends.size();
   }
   g_stats.traceCount = totalTracesCount;
}

void Profiler::fetchClientData()
{
   HOP_PROF_FUNC();

   _server.getPendingData(_serverPendingData);

   if( _recording )
   {
      for( size_t i = 0; i <_serverPendingData.stringData.size(); ++i )
      {
         addStringData( _serverPendingData.stringData[i] );
      }
      for( size_t i = 0; i <_serverPendingData.traces.size(); ++i )
      {
         addTraces(_serverPendingData.traces[i], _serverPendingData.tracesThreadIndex[i] );
      }
      for( size_t i = 0; i < _serverPendingData.lockWaits.size(); ++i )
      {
         addLockWaits(_serverPendingData.lockWaits[i], _serverPendingData.lockWaitThreadIndex[i] );
      }
      for (size_t i = 0; i < _serverPendingData.unlockEvents.size(); ++i)
      {
          addUnlockEvents(_serverPendingData.unlockEvents[i], _serverPendingData.unlockEventsThreadIndex[i]);
      }
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

void Profiler::addLockWaits( const DisplayableLockWaits& lockWaits, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracesPerThread.size() )
   {
      _tracesPerThread.resize( threadIndex + 1 );
   }

   if( !lockWaits.ends.empty() )
   {
      _tracesPerThread[threadIndex].addLockWaits( lockWaits );
   }
}

void Profiler::addUnlockEvents( const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex )
{
   HOP_PROF_FUNC();
   // Check if new thread
   if ( threadIndex >= _tracesPerThread.size() )
   {
      _tracesPerThread.resize( threadIndex + 1 );
   }

   if( !unlockEvents.empty() )
   {
      _tracesPerThread[threadIndex].addUnlockEvents( unlockEvents );
   }
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
}

static bool ptInRect( const ImVec2& pt, const ImVec2& a, const ImVec2& b )
{
   if( pt.x < a.x || pt.x > b.x ) return false;
   if( pt.y < a.y || pt.y > b.y ) return false;

   return true;
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
       ptInRect(
           mousePos,
           drawPos,
           ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + TOOLBAR_BUTTON_HEIGHT ) );

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
       ptInRect(
           mousePos,
           drawPos,
           ImVec2( drawPos.x + TOOLBAR_BUTTON_WIDTH, drawPos.y + TOOLBAR_BUTTON_HEIGHT ) );

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
   switch ( state )
   {
      case hop::SharedMemory::NOT_CONNECTED:
         col = ImColor( 0.8f, 0.0f, 0.0f );
         break;
      case hop::SharedMemory::CONNECTED:
         col = ImColor( 0.0f, 0.8f, 0.0f );
         break;
      case hop::SharedMemory::CONNECTED_NO_CLIENT:
         col = ImColor( 0.8f, 0.8f, 0.0f );
         break;
      case hop::SharedMemory::PERMISSION_DENIED:
         col = ImColor( 0.6f, 0.2f, 0.0f );
         break;
      case hop::SharedMemory::UNKNOWN_CONNECTION_ERROR:
         col = ImColor( 0.4f, 0.0f, 0.0f );
         break;
   }
   ImDrawList* DrawList = ImGui::GetWindowDrawList();
   DrawList->AddCircleFilled( drawPos, 10.0f, col );
}

void hop::Profiler::drawSearchWindow()
{
   HOP_PROF_FUNC();
   bool inputFocus = false;
   if ( _focusSearchWindow && _searchWindowOpen )
   {
      ImGui::SetNextWindowFocus();
      inputFocus = true;
      _focusSearchWindow = false;
   }

   if ( _searchWindowOpen )
   {
      ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.20f, 0.20f, 0.20f, 0.75f ) );
      ImGui::SetNextWindowSize( ImVec2( 600, 300 ), ImGuiSetCond_FirstUseEver );
      if ( ImGui::Begin( "Search Window", &_searchWindowOpen ) )
      {
         static char input[512];

         if ( inputFocus ) ImGui::SetKeyboardFocusHere();

         if ( ImGui::InputText(
                  "Search",
                  input,
                  sizeof( input ),
                  ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue ) &&
              strlen( input ) > 0 )
         {
            const auto startSearch = std::chrono::system_clock::now();

            findTraces( input, _strDb, _tracesPerThread, _searchRes );

            const auto endSearch = std::chrono::system_clock::now();
            hop::g_stats.searchTimeMs =
                std::chrono::duration<double, std::milli>( ( endSearch - startSearch ) ).count();
         }

         auto selection = drawSearchResult( _searchRes, _timeline, _strDb, _tracesPerThread );

         if ( selection.selectedTraceIdx != (size_t)-1 && selection.selectedThreadIdx != (uint32_t)-1 )
         {
            const auto& threadInfo = _tracesPerThread[selection.selectedThreadIdx];
            const TimeStamp absEndTime = threadInfo._traces.ends[selection.selectedTraceIdx];
            const TimeStamp delta = threadInfo._traces.deltas[selection.selectedTraceIdx];
            const TDepth_t depth = threadInfo._traces.depths[selection.selectedTraceIdx];

            // If the thread was hidden, display it so we can see the selected trace
            _tracesPerThread[selection.selectedThreadIdx]._hidden = false;

            const TimeStamp startTime = absEndTime - delta - _timeline.absoluteStartTime();
            const float verticalPosPxl = threadInfo._localTracesVerticalStartPos + (depth * Timeline::PADDED_TRACE_SIZE) - (3* Timeline::PADDED_TRACE_SIZE);
            _timeline.pushNavigationState();
            _timeline.frameToTime( startTime, delta );
            _timeline.moveVerticalPositionPxl( verticalPosPxl );
         }

         if( selection.hoveredTraceIdx != (size_t)-1 && selection.hoveredThreadIdx != (uint32_t)-1 )
         {
            _timeline.addTraceToHighlight( std::make_pair( selection.hoveredTraceIdx, selection.hoveredThreadIdx ) );
         }
      }
      ImGui::End();
      ImGui::PopStyleColor();
   }
}

void hop::Profiler::drawTraceDetailsWindow()
{
   HOP_PROF_FUNC();
   const auto traceDetailRes = drawTraceDetails( _timeline.getTraceDetails(), _tracesPerThread, _strDb );
   if ( traceDetailRes.isWindowOpen )
   {
       _timeline.setTraceDetailsDisplayed();

       // Add the trace that will need to be highlighted
       std::pair<size_t, size_t> span = visibleIndexSpan(
           _tracesPerThread[traceDetailRes.hoveredThreadIdx]._traces,
           _timeline.absoluteTimelineStart(),
           _timeline.absoluteTimelineEnd() );

       for ( const auto& traceHoveredIdx : traceDetailRes.hoveredTraceIds )
       {
          if ( traceHoveredIdx >= span.first && traceHoveredIdx <= span.second )
          {
             _timeline.addTraceToHighlight(
                 std::make_pair( traceHoveredIdx, traceDetailRes.hoveredThreadIdx ) );
          }
       }
   }
   else
   {
      _timeline.clearTraceDetails();
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

   // These must be done before drawing the traces as we need to highlight
   // traces that might be hovered from this window
   drawSearchWindow();
   drawTraceDetailsWindow();

   const auto toolbarDrawPos = ImGui::GetCursorScreenPos();
   if( drawPlayStopButton( toolbarDrawPos, _recording ) )
   {
      setRecording( !_recording );
   }

   auto deleteTracePos = toolbarDrawPos;
   deleteTracePos.x += (2.0f*TOOLBAR_BUTTON_PADDING) + TOOLBAR_BUTTON_WIDTH;
   if( drawDeleteTracesButton( deleteTracePos, !_tracesPerThread.empty() ) )
   {
      hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&](){ clear(); } );
   }

   auto statusPos = toolbarDrawPos;
   statusPos.x += ImGui::GetWindowWidth() - 25.0f;
   statusPos.y += 5.0f;
   drawStatusIcon( statusPos, _server.connectionState() );

   if( _tracesPerThread.empty() && !_recording )
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

      _timeline.draw( _tracesPerThread, _strDb );
   }

   handleHotkey();

   _timeline.clearHighlightedTraces();

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
   static bool useGlFinish = _server.useGlFinish();

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
         if( ImGui::Checkbox("Use glFinish()", &useGlFinish) )
         {
            _server.setUseGlFinish( useGlFinish );
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
   else if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( 'f' ) )
   {
      _searchWindowOpen = true;
      _focusSearchWindow = true;
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
   else if( ImGui::IsKeyDown( SDLK_DELETE ) && !_tracesPerThread.empty() )
   {
      if( ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && !hop::modalWindowShowing() )
         hop::displayModalWindow( "Delete all traces?", hop::MODAL_TYPE_YES_NO, [&](){ clear(); } );
   }
}

bool hop::Profiler::setRecording(bool recording)
{
   bool success = _server.setRecording(recording);
   if (success)
   {
      _recording = recording;
      if (recording)
         _timeline.setRealtime(true);
   }

   return success;
}

bool hop::Profiler::saveToFile( const char* path )
{
   displayModalWindow( "Saving...", MODAL_TYPE_NO_CLOSE );
   std::thread t( [this, path]() {
      // Compute the size of the serialized data
      const size_t dbSerializedSize = serializedSize( _strDb );
      const size_t timelineSerializedSize = serializedSize( _timeline );
      std::vector<size_t> threadInfosSerializedSize( _tracesPerThread.size() );
      for ( size_t i = 0; i < _tracesPerThread.size(); ++i )
      {
         threadInfosSerializedSize[i] = serializedSize( _tracesPerThread[i] );
      }

      const size_t totalSerializedSize =
          std::accumulate( threadInfosSerializedSize.begin(), threadInfosSerializedSize.end(), size_t{0} ) +
          timelineSerializedSize +
          dbSerializedSize;

      std::vector<char> data( totalSerializedSize );

      size_t index = serialize( _strDb, &data[0] );
      index += serialize( _timeline, &data[index] );
      for ( size_t i = 0; i < _tracesPerThread.size(); ++i )
      {
         index += serialize( _tracesPerThread[i], &data[index] );
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
                               (uint32_t)_tracesPerThread.size()};
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

         std::vector<ThreadInfo> threadInfos( header->threadCount );
         for ( uint32_t j = 0; j < header->threadCount; ++j )
         {
            size_t threadInfoSize = deserialize( &uncompressedData[i], threadInfos[j] );
            addTraces( threadInfos[j]._traces, j );
            addLockWaits( threadInfos[j]._lockWaits, j );
            i += threadInfoSize;
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
   _tracesPerThread.clear();
   _timeline.setAbsoluteStartTime( 0 );
   _timeline.clearTraceDetails();
   _timeline.clearBookmarks();
   _timeline.clearTraceStats();
   clearSearchResult( _searchRes );
   _recording = false;
   g_stats.traceCount = 0;
}

