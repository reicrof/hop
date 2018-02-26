#include "Profiler.h"
#include "imgui/imgui.h"
#include "Lod.h"
#include "Stats.h"
#include "Utils.h"
#include "TraceDetail.h"
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

bool saveAsJson( const char* path, const std::vector< uint32_t >& /*threadsId*/, const std::vector< hop::ThreadInfo >& /*threadTraces*/ )
{
   using namespace rapidjson;

   StringBuffer s;
   PrettyWriter<StringBuffer> writer(s);

   writer.StartObject();
   writer.Key("traceEvents");
   writer.StartArray();
   // for( size_t i = 0; i < threadTraces.size(); ++i )
   // {
   //    const uint32_t threadId = threadsId[i];
   //    const std::vector< char >& strData = threadTraces[i].stringData;
   //    for( const auto& chunk : threadTraces[i].chunks )
   //    {
   //       for( const auto& t : chunk )
   //       {
   //          writer.StartObject();
   //          writer.Key("ts");
   //          writer.Uint64( (uint64_t)t.time );
   //          writer.Key("ph");
   //          writer.String( t.flags & hop::DisplayableTrace::START_TRACE ? "B" : "E" );
   //          writer.Key("pid");
   //          writer.Uint(threadId);
   //          writer.Key("name");
   //          writer.String( &strData[t.fctNameIdx] );
   //          writer.EndObject();
   //       }
   //    }
   // }
   writer.EndArray();
   writer.Key("displayTimeUnit");
   writer.String("ms");
   writer.EndObject();

   std::ofstream of(path);
   of << s.GetString();

   return true;
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

void draw()
{
   for( auto p : _profilers )
   {
      p->update( g_deltaTimeMs );
   }

   for ( auto p : _profilers )
   {
      p->draw();
   }

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

Profiler::Profiler( const std::string& name ) : _name( name )
{
   _server.reset( new Server() );
   _server->start( _name.c_str() );
}

void Profiler::addTraces( const DisplayableTraces& traces, uint32_t threadId )
{
   if ( _recording )
   {
      size_t threadIndex = 0;
      for ( ; threadIndex < _threadsId.size(); ++threadIndex )
      {
         if ( _threadsId[threadIndex] == threadId ) break;
      }

      //Thread id not found so add it
      if ( threadIndex == _threadsId.size() )
      {
         _threadsId.push_back( threadId );
         _tracesPerThread.emplace_back();
      }

      const auto startTime = _timeline.absoluteStartTime();
      if( startTime == 0 || (traces.ends[0] - traces.deltas[0]) < startTime )
        _timeline.setAbsoluteStartTime( (traces.ends[0] - traces.deltas[0]) );

      const auto presentTime = _timeline.absolutePresentTime();
      if( presentTime == 0 || traces.ends.back() > presentTime )
        _timeline.setAbsolutePresentTime( traces.ends.back() );

      _tracesPerThread[threadIndex].addTraces( traces );
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
   _server->getPendingUnlockEvents(pendingUnlockEvents, threadIdsUnlockEvents);
   for (size_t i = 0; i < pendingUnlockEvents.size(); ++i)
   {
       addUnlockEvents(pendingUnlockEvents[i], threadIdsUnlockEvents[i]);
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

      _strDb.addStringData( strData );
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

void Profiler::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadId)
{
    if (_recording)
    {
        size_t i = 0;
        for (; i < _threadsId.size(); ++i)
        {
            if (_threadsId[i] == threadId) break;
        }

        // Thread id not found so add it
        if (i == _threadsId.size())
        {
            _threadsId.push_back(threadId);
            _tracesPerThread.emplace_back();
        }

        _tracesPerThread[i].addUnlockEvents(unlockEvents);
    }
}

Profiler::~Profiler()
{
   _server->stop();
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

void hop::Profiler::draw()
{
   ImGui::SetNextWindowSize(ImVec2(1000,500), ImGuiSetCond_FirstUseEver);
   ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 100, 100 ) );
   if ( !ImGui::Begin( _name.c_str(), nullptr, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse ) )
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

   if( _tracesPerThread.empty() && !_recording )
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
         _timeline.moveToPresentTime( false );
      }

      _timeline.draw( _tracesPerThread, _threadsId, _strDb );
	  if (!drawTraceDetails(_timeline.getTraceDetails(), _tracesPerThread, _strDb))
	  {
		  _timeline.clearTraceDetails();
	  }
   }

   ImGui::End();
   ImGui::PopStyleVar();
}

void hop::Profiler::drawMenuBar()
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
   else if( ImGui::IsKeyReleased( 'r' ) )
   {
      setRecording( !_recording );
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_LEFT ) )
   {
      printf("Left arrow\n");
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_RIGHT ) )
   {
      printf("Right arrow\n");
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_UP ) )
   {
      printf("Up arrow\n");
   }
   else if( ImGui::IsKeyPressed( SDL_SCANCODE_DOWN ) )
   {
      printf("Down arrow\n");
   }
}

void hop::Profiler::setRecording( bool recording )
{
   _recording = recording;
   _server->setRecording( recording );
   if( recording )
   {
      _timeline.setRealtime ( true );
   }
}
