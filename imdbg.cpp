#include "imdbg.h"
#include "imgui/imgui.h"
#include <SDL2/SDL_keycode.h>

// Todo : I dont like this dependency
#include "server.h"

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
#include <stdio.h>

// Used to save the traces as json file
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

namespace
{
static std::chrono::time_point<std::chrono::system_clock> g_Time = std::chrono::system_clock::now();
static GLuint g_FontTexture = 0;
std::vector<vdbg::Profiler*> _profilers;

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

bool saveAsJson( const char* path, const std::vector< uint32_t >& threadsId, const std::vector< vdbg::ThreadTraces >& threadTraces )
{
   using namespace rapidjson;

   StringBuffer s;
   PrettyWriter<StringBuffer> writer(s);

   writer.StartObject();
   writer.Key("traceEvents");
   writer.StartArray();
   for( size_t i = 0; i < threadTraces.size(); ++i )
   {
      const uint32_t threadId = threadsId[i];
      const std::vector< char >& strData = threadTraces[i].stringData;
      for( const auto& chunk : threadTraces[i].chunks )
      {
         for( const auto& t : chunk )
         {
            writer.StartObject();
            writer.Key("ts");
            writer.Uint64( (uint64_t)t.time );
            writer.Key("ph");
            writer.String( t.flags & vdbg::DisplayableTrace::START_TRACE ? "B" : "E" );
            writer.Key("pid");
            writer.Uint(threadId);
            writer.Key("name");
            writer.String( &strData[t.fctNameIdx] );
            writer.EndObject();
         }
      }
   }
   writer.EndArray();
   writer.Key("displayTimeUnit");
   writer.String("ms");
   writer.EndObject();

   std::ofstream of(path);
   of << s.GetString();

   return true;
}

} // end of anonymous namespace

namespace vdbg
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
   io.DeltaTime = static_cast<float>(
       std::chrono::duration_cast<std::chrono::milliseconds>( ( curTime - g_Time ) ).count() ) / 1000.0f;
   g_Time = curTime;

   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
   io.MousePos = ImVec2( (float)mouseX, (float)mouseY );

   io.MouseDown[0] = lmbPressed;
   io.MouseDown[1] = rmbPressed;
   io.MouseWheel = mousewheel;

   // Start the frame
   ImGui::NewFrame();
}

void draw()
{
   static double lastTime = 0.0;

   const auto preDrawTime = std::chrono::system_clock::now();
   for ( auto p : _profilers )
   {
      p->draw();
   }

   ImGui::Text( "Drawing took %f ms", lastTime );

   ImGui::Render();

   const auto postDrawTime = std::chrono::system_clock::now();
   lastTime = std::chrono::duration< double, std::milli>( ( postDrawTime - preDrawTime ) ).count();
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

Profiler::Profiler( const std::string& name ) : _name( name )
{
   _server.reset( new Server() );
   _server->start( _name.c_str() );
}

void Profiler::addTraces( const std::vector<DisplayableTrace>& traces, uint32_t threadId )
{
   if ( _recording )
   {
      size_t i = 0;
      for ( ; i < _threadsId.size(); ++i )
      {
         if ( _threadsId[i] == threadId ) break;
      }

      // Thread id not found so add it
      if ( i == _threadsId.size() )
      {
         _threadsId.push_back( threadId );
         _tracesPerThread.emplace_back();
      }

      const auto startTime = _timeline.absoluteStartTime();
      if( startTime == 0 || traces[0].time < startTime )
        _timeline.setAbsoluteStartTime( traces[0].time );

      const auto presentTime = _timeline.absoluteStartTime();
      if( presentTime == 0 || traces.back().time > presentTime )
        _timeline.setAbsolutePresentTime( traces.back().time );

      _tracesPerThread[i].addTraces( traces );
   }
}

void Profiler::fetchClientData()
{
   // TODO: rethink and redo this part
   _server->getPendingProfilingTraces( pendingTraces, stringData, threadIds );
   for( size_t i = 0; i < pendingTraces.size(); ++i )
   {
      addTraces( pendingTraces[i], threadIds[i] );
      addStringData( stringData[i], threadIds[i] );
   }
   _server->getPendingLockWaits( pendingLockWaits, threadIdsLockWaits );
   for( size_t i = 0; i < pendingLockWaits.size(); ++i )
   {
      addLockWaits( pendingLockWaits[i], threadIdsLockWaits[i] );
   }
}

void Profiler::addStringData( const std::vector<char>& strData, uint32_t threadId )
{
   // We should read the string data even when not recording since the string data
   // is sent only once (the first time a function is used)
   if( !strData.empty() )
   {
      size_t i = 0;
      for( ; i < _threadsId.size(); ++i )
      {
         if( _threadsId[i] == threadId ) break;
      }
      
      // Thread id not found so add it
      if( i == _threadsId.size() )
      {
         _threadsId.push_back( threadId );
         _tracesPerThread.emplace_back();
      }

      _tracesPerThread[i].stringData.insert( _tracesPerThread[i].stringData.end(), strData.begin(), strData.end() );
   }
}

void Profiler::addLockWaits( const std::vector<LockWait>& lockWaits, uint32_t threadId )
{
   if ( _recording )
   {
      size_t i = 0;
      for ( ; i < _threadsId.size(); ++i )
      {
         if ( _threadsId[i] == threadId ) break;
      }

      // Thread id not found so add it
      if ( i == _threadsId.size() )
      {
         _threadsId.push_back( threadId );
         _tracesPerThread.emplace_back();
      }

      _tracesPerThread[i].addLockWaits( lockWaits );
   }
}

Profiler::~Profiler()
{
   _server->stop();
}

ThreadTraces::ThreadTraces()
{
   chunks.reserve( CHUNK_SIZE );
   startTimes.reserve( CHUNK_SIZE );
   endTimes.reserve( CHUNK_SIZE );
}

void ThreadTraces::addTraces( const std::vector< DisplayableTrace >& traces )
{
   // The traces should already be sorted.
   assert( std::is_sorted( traces.begin(), traces.end() ) );

   // Check if we have enough space in the chunk to append these traces. If
   // not, create a new chunk
   if( chunks.empty() || CHUNK_SIZE - (int)chunks.back().size() < (int)traces.size() )
   {
      // Create new chunk that contains the traces and add the start time
      // of the chunk to our reference list.
      chunks.emplace_back();
      chunks.back().assign( traces.begin(), traces.end() );
      startTimes.push_back( traces.front().time );
      endTimes.push_back( traces.back().time );
   }
   else
   {
      const auto prevSize = chunks.back().size();
      chunks.back().insert( chunks.back().end(), traces.begin(), traces.end() );

      // The first trace added has the smallest time (since they are already sorted).
      // Thus all the traces with a bigger time than that could now be in an unsorted state.
      // We need to sort all the traces that have a time >= to this trace
      auto it = std::lower_bound(
          chunks.back().begin(), chunks.back().begin() + prevSize, traces.front() );
      std::sort( it, chunks.back().end() );

      // Update the endTime
      endTimes[ chunks.size()-1 ] = chunks.back().back().time;
   }

   // The starttime and endtimes should always be sorted, otherwise we need to investigate why it is not...
   assert( std::is_sorted( startTimes.begin(), startTimes.end() ) );
   assert( std::is_sorted( endTimes.begin(), endTimes.end() ) );
}

void ThreadTraces::addLockWaits( const std::vector< LockWait >& lockWaits )
{
   _lockWaits.insert( _lockWaits.end(), lockWaits.begin(), lockWaits.end() );
}

} // end of namespace vdbg


// static bool drawDispTrace( const vdbg::DisplayableTraceFrame& frame, size_t& i )
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
static bool ptInRect( const ImVec2& pt, const ImVec2& a, const ImVec2& b )
{
   if( pt.x < a.x || pt.x > b.x ) return false;
   if( pt.y < a.y || pt.y > b.y ) return false;

   return true;
}

static bool drawPlayStopButton( bool& isRecording )
{
   constexpr float height = 15.0f, width = 15.0f, padding = 5.0f;
   const auto startDrawPos = ImGui::GetCursorScreenPos();
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const auto& mousePos = ImGui::GetMousePos();
   const bool hovering = ptInRect( mousePos, startDrawPos, ImVec2( startDrawPos.x + width, startDrawPos.y + height ) );

   if( isRecording )
   {
      DrawList->AddRectFilled( startDrawPos, ImVec2( startDrawPos.x + width, startDrawPos.y + height ), hovering ? ImColor(0.9f,0.0f,0.0f) : ImColor(0.7f,0.0f,.0f) );
   }
   else
   {
      ImVec2 pts[] = {startDrawPos,
                      ImVec2( startDrawPos.x + width, startDrawPos.y + ( height * 0.5 ) ),
                      ImVec2( startDrawPos.x, startDrawPos.y + width )};
      DrawList->AddConvexPolyFilled( pts, 3, hovering ? ImColor( 0.0f, 0.9f, 0.0f ) : ImColor( 0.0f, 0.7f, 0.0f ), true );
   }

   ImGui::SetCursorScreenPos( ImVec2(startDrawPos.x, startDrawPos.y + height + padding) );

   return hovering && ImGui::IsMouseClicked(0);
}

void vdbg::Profiler::draw()
{
   //ImGui::SetNextWindowSize(ImVec2(700,500), ImGuiSetCond_FirstUseEver);
   ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 100, 100 ) );
   if ( !ImGui::Begin( _name.c_str(), nullptr, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar ) )
   {
      // Early out
      ImGui::End();
      ImGui::PopStyleVar();
      return;
   }

   handleHotkey();

   drawMenuBar();

   if( drawPlayStopButton( _recording ) )
   {
      setRecording( !_recording );
   }

   if( _tracesPerThread.empty() )
   {
      const char* record = "To start recording press 'r'";
      const auto pos = ImGui::GetWindowPos();
      const float windowWidthPxl = ImGui::GetWindowWidth();
      const float windowHeightPxl = ImGui::GetWindowHeight();
      ImDrawList* DrawList = ImGui::GetWindowDrawList();
      auto size = ImGui::CalcTextSize( record );
      DrawList->AddText( ImGui::GetIO().Fonts->Fonts[0], 30.0f, ImVec2(pos.x + windowWidthPxl/2 - (size.x), pos.y + windowHeightPxl/2),ImGui::GetColorU32( ImGuiCol_TextDisabled ), record );
   }
   else
   {
      //  Move timeline to the most recent trace if Live mode is on
      if( _recording && _timeline.realtime() )
      {
         _timeline.moveToPresentTime();
      }

      _timeline.draw( _tracesPerThread, _threadsId );
   }

   ImGui::End();
   ImGui::PopStyleVar();
}

void vdbg::Profiler::drawMenuBar()
{
   const char* const menuSaveAsJason = "Save as JSON";
   const char* const menuHelp = "Help";
   const char* menuAction = NULL;
   if ( ImGui::BeginMenuBar() )
   {
      if ( ImGui::BeginMenu( "Menu" ) )
      {
         if ( ImGui::MenuItem( menuSaveAsJason, NULL ) )
         {
            menuAction = menuSaveAsJason;
         }
         if( ImGui::MenuItem( menuHelp, NULL ) )
         {
            menuAction = menuHelp;
         }
         if ( ImGui::MenuItem( "Exit", NULL ) )
         {
            exit(0);
         }
         ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
   }

   if ( menuAction == menuSaveAsJason )
   {
      ImGui::OpenPopup( menuSaveAsJason );
   }
   else if( menuAction == menuHelp )
   {
      ImGui::OpenPopup( menuHelp );
   }

   if ( ImGui::BeginPopupModal( menuSaveAsJason, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
   {
      static char path[512] = {};
      ImGui::InputText( "Save to", path, sizeof( path ) );
      ImGui::Separator();

      if ( ImGui::Button( "Save", ImVec2( 120, 0 ) ) )
      {
         saveAsJson( path, _threadsId, _tracesPerThread );
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
      ImGui::Text("This is a help menu\nPretty useful isnt it?\n\n");
      if ( ImGui::Button( "Yes indeed", ImVec2( 120, 0 ) ) )
      {
         ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
   }
}

void vdbg::Profiler::handleHotkey()
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
   else if( ImGui::IsKeyReleased( 'r' ) )
   {
      setRecording( !_recording );
   }
}

void vdbg::Profiler::setRecording( bool recording )
{
   _recording = recording;
   _server->setRecording( recording );
   if( recording )
   {
      _timeline.setRealtime ( true );
   }
}

// TODO template these 2 functions so they can be used with different time ratios
template< typename T = uint64_t >
static inline T microsToPxl( float windowWidth, int64_t usToDisplay, int64_t us )
{
   const double usPerPxl = usToDisplay / windowWidth;
   return static_cast<T>( (double)us / usPerPxl );
}

template< typename T = uint64_t >
static inline T pxlToMicros( float windowWidth, int64_t usToDisplay, int64_t pxl )
{
   const double usPerPxl = usToDisplay / windowWidth;
   return static_cast<T>( usPerPxl * (double)pxl );
}

void vdbg::ProfilerTimeline::draw(
    const std::vector<ThreadTraces>& tracesPerThread,
    const std::vector<uint32_t>& threadIds )
{
   const auto startDrawPos = ImGui::GetCursorScreenPos();
   drawTimeline( startDrawPos.x, startDrawPos.y + 5 );

   ImGui::BeginChild( "Traces", ImVec2(0,0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
   char threadName[128] = "Thread ";
   for ( size_t i = 0; i < tracesPerThread.size(); ++i )
   {
      snprintf(
          threadName + sizeof( "Thread" ),
          sizeof( threadName ),
          "%lu (id=%u)",
          i,
          threadIds[i] );
      ImGui::PushStyleColor( ImGuiCol_Button, ImColor::HSV( i / 7.0f, 0.6f, 0.6f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor::HSV( i / 7.0f, 0.6f, 0.6f ) );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor::HSV( i / 7.0f, 0.6f, 0.6f ) );
      ImGui::Button( threadName );
      ImGui::PopStyleColor( 3 );
      ImGui::Spacing();
      ImGui::Separator();

      const auto curPos = ImGui::GetCursorScreenPos();
      drawTraces( tracesPerThread[i], i, curPos.x, curPos.y );
      drawLockWaits( tracesPerThread[i], curPos.x, curPos.y );

      ImGui::InvisibleButton( "trace-padding", ImVec2( 20, 40 ) );
   }

   ImGui::EndChild();

   if ( ImGui::IsItemHoveredRect() )
   {
     ImVec2 mousePosInCanvas =
         ImVec2( ImGui::GetIO().MousePos.x - startDrawPos.x, ImGui::GetIO().MousePos.y - startDrawPos.y );
     handleMouseDrag( mousePosInCanvas.x, mousePosInCanvas.y );
     handleMouseWheel( mousePosInCanvas.x, mousePosInCanvas.y );
   }
}

void vdbg::ProfilerTimeline::drawTimeline( const float posX, const float posY )
{
   constexpr int64_t minStepSize = 10;
   constexpr int64_t minStepCount = 20;
   constexpr int64_t maxStepCount = 140;

   const float windowWidthPxl = ImGui::GetWindowWidth();

   const size_t stepsCount = [this, minStepSize]()
   {
      size_t stepsCount = _microsToDisplay / _stepSizeInMicros;
      while( stepsCount > maxStepCount || (stepsCount < minStepCount && _stepSizeInMicros > minStepSize) )
      {
        if( stepsCount > maxStepCount )
        {
           if( _stepSizeInMicros == minStepSize ) { _stepSizeInMicros = 8000; }
           _stepSizeInMicros *= 5.0f;
        }
        else if( stepsCount < minStepCount )
        {
           _stepSizeInMicros /= 5.0f;
           _stepSizeInMicros = std::max( _stepSizeInMicros, minStepSize );
        }
        stepsCount = _microsToDisplay / _stepSizeInMicros;
      }
      return stepsCount;
   }();

   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   // Start drawing the vertical lines on the timeline
   constexpr float smallLineLength = 10.0f;
   constexpr float deltaBigLineLength = 12.0f; // The diff between the small line and big one
   constexpr float deltaMidLineLength = 7.0f; // The diff between the small line and mid one

   const int64_t stepSizePxl = microsToPxl( windowWidthPxl, _microsToDisplay, _stepSizeInMicros );
   const int64_t stepsDone = _startMicros / _stepSizeInMicros;
   const int64_t remainder = _startMicros % _stepSizeInMicros;
   int remainderPxl = 0;
   if( remainder != 0 )
      remainderPxl = microsToPxl( windowWidthPxl, _microsToDisplay, remainder );

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top( posX, posY );
   top.x -= (stepSizePxl + remainderPxl) - stepSizePxl ;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int count = stepsDone;
   std::vector< std::pair< ImVec2, double > > textPos;
   const auto maxPosX = posX + windowWidthPxl;
   for( double i = top.x; i < maxPosX; i += stepSizePxl, ++count )
   {
      // Draw biggest begin/end lines
      if( count % 10 == 0 )
      {
         auto startEndLine = bottom;
         startEndLine.y += deltaBigLineLength;
         DrawList->AddLine( top, startEndLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 3.0f );
         textPos.emplace_back( ImVec2( startEndLine.x, startEndLine.y + 5.0f ), count * _stepSizeInMicros );
      }
      // Draw midline
      else if( count % 5 == 0 )
      {
          auto midLine = bottom;
          midLine.y += deltaMidLineLength;
          DrawList->AddLine( top, midLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.5f );
      }
      else
      {
        DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f ); 
      }

      top.x += stepSizePxl;
      bottom.x += stepSizePxl;
   }

   // Draw horizontal line
   DrawList->AddLine( ImVec2( posX, posY ), ImVec2( posX + windowWidthPxl, posY ), ImGui::GetColorU32(ImGuiCol_Border) );

   const int64_t total = stepsCount * _stepSizeInMicros;
   if( total < 1000 )
   {
        // print as microsecs
       for( const auto& pos : textPos )
       {
          ImGui::SetCursorScreenPos( pos.first );
          ImGui::Text( "%d us", (int)pos.second );
       }
   }
   else if( total < 1000000 )
   {
    // print as milliseconds
     for( const auto& pos : textPos )
     {
        ImGui::SetCursorScreenPos( pos.first );
        ImGui::Text( "%.3f ms", (float)(pos.second)/1000.0f );
     }
   }
   else if( total < 1000000000 )
   {
       // print as seconds
     for( const auto& pos : textPos )
     {
        ImGui::SetCursorScreenPos( pos.first );
        ImGui::Text( "%.3f s", (float)(pos.second)/1000000.0f );
     }
   }

   ImGui::SetCursorScreenPos( ImVec2{ posX, posY + 50.0f } );
}

void vdbg::ProfilerTimeline::handleMouseWheel( float mousePosX, float )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   ImGuiIO& io = ImGui::GetIO();
   if ( io.MouseWheel > 0 )
   {
      zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosX ) + _startMicros, 0.9f );
   }
   else if ( io.MouseWheel < 0 )
   {
      zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosX ) + _startMicros, 1.1f );
   }
}

void vdbg::ProfilerTimeline::handleMouseDrag( float mouseInCanvasX, float mouseInCanvasY )
{
  // Left mouse button dragging
  if( ImGui::IsMouseDragging( 0 ) )
  {
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const auto delta = ImGui::GetMouseDragDelta();
    const int64_t deltaXInMicros = pxlToMicros<int64_t>( windowWidthPxl, _microsToDisplay, delta.x );
    _startMicros -= deltaXInMicros;

    // Switch to the traces context to modify the scroll
    ImGui::BeginChild("Traces");
    ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
    ImGui::EndChild();

    ImGui::ResetMouseDragDelta();
    setRealtime ( false );
  }
  // Right mouse button dragging
  else if( ImGui::IsMouseDragging( 1 ) )
  {
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const auto delta = ImGui::GetMouseDragDelta( 1 );

    const auto curMousePosInScreen = ImGui::GetMousePos();
    DrawList->AddRectFilled(
        ImVec2( curMousePosInScreen.x, 0 ),
        ImVec2( curMousePosInScreen.x - delta.x, ImGui::GetWindowHeight() ),
        ImColor( 255, 255, 255, 64 ) );
  }

  if( ImGui::IsMouseClicked(1) )
  {
    _rightClickStartPosInCanvas[0] = mouseInCanvasX;
    _rightClickStartPosInCanvas[1] = mouseInCanvasY;
    setRealtime ( false );
  }

  // Handle right mouse click up. (Finished right click selection zoom)
  if( ImGui::IsMouseReleased(1) )
  {
    const float minX = std::min( _rightClickStartPosInCanvas[0], mouseInCanvasX );
    const float maxX = std::max( _rightClickStartPosInCanvas[0], mouseInCanvasX );
    const float windowWidthPxl = ImGui::GetWindowWidth();
    const int64_t minXinMicros = pxlToMicros<int64_t>( windowWidthPxl, _microsToDisplay, minX );
    _startMicros += minXinMicros;
    _microsToDisplay = pxlToMicros<int64_t>( windowWidthPxl, _microsToDisplay, maxX - minX );

    // Reset position
    _rightClickStartPosInCanvas[0] = _rightClickStartPosInCanvas[1] = 0.0f;
  }
}

bool vdbg::ProfilerTimeline::realtime() const noexcept
{
   return _realtime;
}

void vdbg::ProfilerTimeline::setRealtime( bool isRealtime ) noexcept
{
   _realtime = isRealtime;
}

vdbg::TimeStamp vdbg::ProfilerTimeline::absoluteStartTime() const noexcept
{
   return _absoluteStartTime;
}

vdbg::TimeStamp vdbg::ProfilerTimeline::absolutePresentTime() const noexcept
{
   return _absolutePresentTime;
}

void vdbg::ProfilerTimeline::setAbsoluteStartTime( TimeStamp time ) noexcept
{
   _absoluteStartTime = time;
}

void vdbg::ProfilerTimeline::setAbsolutePresentTime( TimeStamp time ) noexcept
{
   _absolutePresentTime = time;
}

void vdbg::ProfilerTimeline::moveToTime( int64_t timeInMicro ) noexcept
{
   _startMicros = timeInMicro - (_microsToDisplay / 2);
}

void vdbg::ProfilerTimeline::moveToStart() noexcept
{
   moveToTime( _microsToDisplay * 0.5f );
   setRealtime ( false );
}

void vdbg::ProfilerTimeline::moveToPresentTime() noexcept
{
   moveToTime( (_absolutePresentTime - _absoluteStartTime) / 1000 );
}

void vdbg::ProfilerTimeline::zoomOn( int64_t microToZoomOn, float zoomFactor )
{
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const size_t microToZoom = microToZoomOn - _startMicros;
   const auto prevMicrosToDisplay = _microsToDisplay;
   _microsToDisplay *= zoomFactor;

   const int64_t prevPxlPos = microsToPxl( windowWidthPxl, prevMicrosToDisplay, microToZoom );
   const int64_t newPxlPos = microsToPxl( windowWidthPxl, _microsToDisplay, microToZoom );

   const int64_t pxlDiff = newPxlPos - prevPxlPos;
   if ( pxlDiff != 0 )
   {
      const int64_t timeDiff = pxlToMicros( windowWidthPxl, _microsToDisplay, pxlDiff );
      _startMicros += timeDiff;
   }
}

void vdbg::ProfilerTimeline::drawTraces( const ThreadTraces& traces, int threadIndex, const float posX, const float posY )
{
   static constexpr float MIN_TRACE_LENGTH_PXL = 0.25f;

   if( traces.startTimes.empty() ) return;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t startMicrosAsPxl = microsToPxl<int64_t>( windowWidthPxl, _microsToDisplay, _startMicros );

   std::vector< ImVec2 > pos;
   std::vector< float > length;
   std::vector< const DisplayableTrace* > tracesToDraw;
   pos.reserve( 128 );
   tracesToDraw.reserve( 128 );

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + (_startMicros * 1000);
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + ( _microsToDisplay * 1000 );

   const auto it1 = std::lower_bound(
       traces.endTimes.begin(), traces.endTimes.end(), firstTraceAbsoluteTime );
   const auto it2 = std::upper_bound(
       traces.startTimes.begin(), traces.startTimes.end(), lastTraceAbsoluteTime );
   const size_t firstChunkIndex = std::distance( traces.endTimes.begin(), it1 );
   const size_t lastChunkIndex = std::distance( traces.startTimes.begin(), it2 );

   int curDepth = -1;
   int maxDepth = 0;
   for( size_t i = firstChunkIndex; i < lastChunkIndex; ++i )
   {
      const auto& chunkTraces = traces.chunks[ i ];
      for ( const auto& t : chunkTraces )
      {
         if ( t.isStartTrace() )
         {
            ++curDepth;
            if ( t.time > firstTraceAbsoluteTime ||
                 t.time + ( t.deltaTime ) > firstTraceAbsoluteTime )
            {
               // The start time is bigger than the maximum time on the timeline. We are done
               // drawing the traces.
               if ( t.time > lastTraceAbsoluteTime ) break;

               const int64_t traceStartInMicros = ((t.time - absoluteStart) / 1000);
               maxDepth = std::max( curDepth, maxDepth );
               const auto traceStartPxl =
                   microsToPxl<float>( windowWidthPxl, _microsToDisplay, traceStartInMicros );
               const float traceLengthPxl =
                   microsToPxl<float>( windowWidthPxl, _microsToDisplay, t.deltaTime / 1000 );

               // Skip trace if it is way smaller than treshold
               if( traceLengthPxl < MIN_TRACE_LENGTH_PXL )
                     continue;

               pos.push_back( ImVec2(
                   posX - startMicrosAsPxl + traceStartPxl,
                   posY + curDepth * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING ) ) );
               length.push_back( traceLengthPxl );
               tracesToDraw.push_back( &t );
            }
         }
         else
         {
            --curDepth;
         }
      }
   }

   _maxTraceDepthPerThread[ threadIndex ] = std::max( _maxTraceDepthPerThread[ threadIndex ], maxDepth );

   char curName[ 512 ] = {};
   for( size_t i = 0; i < tracesToDraw.size(); ++i )
   {
      const DisplayableTrace& t = *tracesToDraw[i];
      if ( t.classNameIdx > 0 )
      {
         // We do have a class name. Prepend it to the string
         snprintf(
             curName,
             sizeof( curName ),
             "%s::%s",
             &traces.stringData[t.classNameIdx],
             &traces.stringData[t.fctNameIdx] );
      }
      else
      {
         // No class name. Ignore it
         snprintf( curName, sizeof( curName ), "%s", &traces.stringData[t.fctNameIdx] );
      }

      ImGui::SetCursorScreenPos( pos[i] );
      ImGui::Button( curName, ImVec2(length[i],TRACE_HEIGHT) );
      if ( length[i] > 3 && ImGui::IsItemHovered() )
      {
         size_t lastChar = strlen( curName );
         curName[ lastChar ] = ' ';
         ImGui::BeginTooltip();
         snprintf(
             curName + lastChar,
             sizeof( curName ) - lastChar,
             "(%.3f ms) \n   %s:%d ",
             ( t.deltaTime / 1000000.0f ),
             &traces.stringData[t.fileNameIdx],
             t.lineNb );
         ImGui::TextUnformatted(curName);
         ImGui::EndTooltip();
      }
   }

   ImGui::SetCursorScreenPos(
       ImVec2{posX, posY + _maxTraceDepthPerThread[ threadIndex ] * ( TRACE_HEIGHT + TRACE_VERTICAL_PADDING )} );
}

void vdbg::ProfilerTimeline::drawLockWaits( const ThreadTraces& traces, const float posX, const float posY )
{
   if( traces.startTimes.empty() ) return;

   const auto& lockWaits = traces._lockWaits;

   const auto absoluteStart = _absoluteStartTime;
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const int64_t startMicrosAsPxl = microsToPxl<int64_t>( windowWidthPxl, _microsToDisplay, _startMicros );

   // The time range to draw in absolute time
   const TimeStamp firstTraceAbsoluteTime = absoluteStart + (_startMicros * 1000);
   const TimeStamp lastTraceAbsoluteTime = firstTraceAbsoluteTime + ( _microsToDisplay * 1000 );

   ImGui::PushStyleColor(ImGuiCol_Button, ImColor(1, 0, 0));
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(1, 0, 0));
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(1, 0, 0));
   for( const auto& lw: lockWaits )
   {
      if ( lw.end >= firstTraceAbsoluteTime && lw.start <= lastTraceAbsoluteTime )
      {
         const int64_t startInMicros = ( ( lw.start - absoluteStart ) / 1000 );

         const auto startPxl =
             microsToPxl<float>( windowWidthPxl, _microsToDisplay, startInMicros );
         const float lengthPxl =
             microsToPxl<float>( windowWidthPxl, _microsToDisplay, (lw.end - lw.start) / 1000.0f );

         // Skip if it is way smaller than treshold
         if ( lengthPxl < 0.25 ) continue;

         ImGui::SetCursorScreenPos(
             ImVec2( posX - startMicrosAsPxl + startPxl, posY ) );
         ImGui::Button( "Lock", ImVec2(lengthPxl, 20.f) );
      }
   }
   ImGui::PopStyleColor(3);
}