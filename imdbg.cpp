#include "imdbg.h"
#include "imgui/imgui.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cassert>
#include <chrono>
#include <vector>
#include <algorithm>
#include <stdio.h>

static void saveAsJson( const char* path, const vdbg::DisplayableTraceFrame& frame );

namespace
{
static std::chrono::time_point<std::chrono::system_clock> g_Time = std::chrono::system_clock::now();
static GLuint g_FontTexture = 0;
std::vector<imdbg::Profiler> _profilers;

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

// bool ShowHelpMarker(const char* desc)
// {
//     ImGui::TextDisabled("(?)");
//     if (ImGui::IsItemHovered())
//     {
//         ImGui::BeginTooltip();
//         ImGui::PushTextWrapPos(450.0f);
//         ImGui::TextUnformatted(desc);
//         ImGui::PopTextWrapPos();
//         ImGui::EndTooltip();
//     }

//     return true;
// }

} // end of anonymous namespace

namespace imdbg
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
   //static const auto preDrawTime = std::chrono::system_clock::now();

   for ( auto& p : _profilers )
   {
      p.draw();
   }

   ImGui::Render();

   //static const auto postDrawTime = std::chrono::system_clock::now();
   //size_t milis = std::chrono::duration_cast<std::chrono::milliseconds>( ( postDrawTime - preDrawTime ) ).count();
   //printf( "immediate gui draw took : %zu\n", milis );
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
                                           // Mouse callbacks
   // glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
   // glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
}

imdbg::Profiler* newProfiler( const std::string& name )
{
   static bool first = true;
   if( first )
   {
      _profilers.reserve( 128 );
      first = false;
   }
   imdbg::Profiler prof(name);
   _profilers.emplace_back( std::move( prof ) );
   return &_profilers.back();
}

Profiler::Profiler( const std::string& name ) : _name( name )
{
}

void Profiler::draw()
{
   ImGui::SetNextWindowSize(ImVec2(700,500), ImGuiSetCond_FirstUseEver);
   if ( !ImGui::Begin( _name.c_str() ) )
   {
      // Early out
      ImGui::End();
      return;
   }

   for( size_t i = 0; i < _threadsId.size(); ++i )
   {
      std::string headerName( "Thread " + std::to_string( _threadsId[i] ) );
      if( ImGui::CollapsingHeader( headerName.c_str() ) )
      {
         auto& threadTrace = _tracesPerThread[i];
         ImGui::PushID( &threadTrace );
         threadTrace.draw();
         ImGui::PopID();
         ImGui::Spacing();
         ImGui::Spacing();
         ImGui::Spacing();
      }
   }

   ImGui::End();
}

void Profiler::addTraces( const vdbg::DisplayableTraceFrame& traces )
{
   if( _recording )
   {
      size_t i = 0;
      for( ; i < _threadsId.size(); ++i )
      {
         if( _threadsId[i] == traces.threadId ) break;
      }
      
      if( i < _threadsId.size() )
      {
         _tracesPerThread[i].addTraces( traces );
      }
      else
      {
         _threadsId.push_back( traces.threadId );
         _tracesPerThread.emplace_back( vdbg::ThreadTraces{});
         _tracesPerThread.back().addTraces( traces );
      }
   }
}

} // end of namespace imdbg

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

static void saveAsJson( const char* path, const vdbg::DisplayableTraceFrame& frame )
{
   using namespace rapidjson;

   StringBuffer s;
   PrettyWriter<StringBuffer> writer(s);
   printf( "Should save as json\n" );
   writer.StartObject();
   writer.Key("traceEvents");
   writer.StartArray();
   const auto& traces = frame.traces;
   const uint32_t threadId = frame.threadId;
   for( const auto& t : traces )
   {
      writer.StartObject();
      writer.Key("ts");
      writer.Double( (double)t.time );
      writer.Key("ph");
      writer.String( t.flags ? "B" : "E" );
      writer.Key("pid");
      writer.Uint(threadId);
      writer.Key("name");
      writer.String( t.name );
      writer.EndObject();
   }
   writer.EndArray();
   writer.Key("displayTimeUnit");
   writer.String("ms");
   writer.EndObject();

   std::ofstream of(path);
   of << s.GetString();

}

void vdbg::ThreadTraces::addTraces(const vdbg::DisplayableTraceFrame& traces )
{
   _dispTraces.emplace_back( traces );
}


static bool drawDispTrace( const vdbg::DisplayableTraceFrame& frame, size_t& i )
{
   const auto& trace = frame.traces[i];
   if( !trace.flags ) return false;

   bool isOpen = ImGui::TreeNode( trace.name, "%s :    %f ms", trace.name, trace.deltaTime );
   if( isOpen )
   {
      ++i;
      while( frame.traces[i].flags )
      {
         drawDispTrace( frame, i );
      }
      ImGui::TreePop();
      ++i;
   }
   else
   {
      int lvl = 0;
      for ( size_t j = i + 1; j < frame.traces.size(); ++j )
      {
         if ( frame.traces[j].flags )
         {
            ++lvl;
         }
         else
         {
            --lvl;
            if ( lvl < 0 )
            {
               i = std::min( ++j, frame.traces.size()-1);
               break;
            }
         }
      }
      assert( i < frame.traces.size() );
   }
   return isOpen;
}

void vdbg::ThreadTraces::draw()
{
   ImGui::Checkbox( "Listening", &_recording );
   ImGui::SameLine();
   ImGui::Checkbox( "Live", &_realTime );

   if ( _realTime && _recording )
   {
      _frameToShow = std::max( ( (int)_dispTraces.size() ) - 1, 0 );
   }

   ImGui::SliderInt( "Frame to show", &_frameToShow, 0, _dispTraces.size() - 1 );

   std::vector<float> values( _frameCountToShow, 0.0f );
   if ( !_dispTraces.empty() )
   {
      int startFrame = std::max( (int)_frameToShow - (int)( _frameCountToShow - 1 ), 0 );
      int count = 0;
      for ( int i = startFrame + 1; i <= (int)_frameToShow; ++i, ++count )
      {
         values[count] = _dispTraces[i].traces.front().deltaTime;
      }
   }

   // int offset = _frameCountToShow;
   // if( offset > (int)values.size() )
   //    offset = 0;

   int pickedFrame = ImGui::PlotHistogram(
       "",
       values.data(),
       values.size(),
       values.size() - 1,
       "Frames (ms)",
       0.001f,
       _maxFrameTime * 1.05f,
       ImVec2{0, 100},
       sizeof( float ),
       values.size() - 1 );

   if ( pickedFrame != -1 )
   {
      _frameToShow -= ( _frameCountToShow - ( pickedFrame + 1 ) );
   }

   if ( ImGui::IsItemHovered() )
   {
      ImGuiIO& io = ImGui::GetIO();
      if ( io.MouseWheel > 0 )
      {
         if ( io.KeyCtrl )
         {
            _maxFrameTime *= 0.95f;
         }
         else
         {
            if ( _frameCountToShow > 6 ) _frameCountToShow -= 2;
         }
      }
      else if ( io.MouseWheel < 0 )
      {
         if ( io.KeyCtrl )
         {
            _maxFrameTime *= 1.05f;
         }
         else
         {
            if ( _frameCountToShow < _dispTraces.size() - 3 ) _frameCountToShow += 2;
         }
      }
   }

   ImGui::DragFloat( "Max value", &_maxFrameTime, 0.005f );

   // Draw the traces
   if ( !_dispTraces.empty() )
   {
      const vdbg::DisplayableTraceFrame& frameToDraw = _dispTraces[_frameToShow];
      for ( size_t i = 0; i < frameToDraw.traces.size(); )
      {
         if ( !drawDispTrace( frameToDraw, i ) )
         {
            ++i;
         }
      }
   }

   ImGui::Spacing();

   static char pathToSave[256] = "";
   ImGui::InputText( "", pathToSave, 256 );
   ImGui::SameLine();
   if ( ImGui::Button( "Save As" ) )
   {
      if ( _frameCountToShow < _dispTraces.size() )
      {
         saveAsJson( pathToSave, _dispTraces[_frameToShow] );
      }
   }
}