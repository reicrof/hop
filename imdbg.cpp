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

bool ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    return true;
}

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

imdbg::Profiler* newProfiler( const char* name )
{
   imdbg::Profiler prof{name};
   _profilers.emplace_back( std::move( prof ) );
   return &_profilers.back();
}

namespace
{
   enum TraceFlags { SHOW_GRAPH = 1, SHOW_GL_MATRICES = 1 << 1};
   bool drawTrace( Profiler::Trace& trace )
   {
      const bool isOpen = ImGui::TreeNode( trace.name, "%s :    %f ms", trace.name, trace.curTime );
      if (ImGui::BeginPopupContextItem(trace.name) )
      {
          if (ImGui::Selectable( (trace.flags & SHOW_GRAPH) ? "Hide graph" : "Show graph"))
            trace.flags ^= SHOW_GRAPH;
          ImGui::SameLine();
          ShowHelpMarker("Enable/Disable the graph history of the current trace");

          if (ImGui::Selectable( (trace.flags & SHOW_GL_MATRICES) ? "Hide GL Matrices" : "Show GL Matrices"))
            trace.flags ^= SHOW_GL_MATRICES;
          ImGui::SameLine();
          ShowHelpMarker("Show/Hide the GL modelview and projection matrices. They are the Matrices at the time when the trace was pushed.");

          ImGui::EndPopup();
      }

      if( isOpen && trace.flags > 0 )
      {
         if( ImGui::CollapsingHeader("More Info") )
         {
            if( trace.flags & SHOW_GRAPH )
            {
              static constexpr float PAD = 0.1f;
              ImGui::PlotLines( "Times", trace.details->prevTimes.data(), (int)trace.details->prevTimes.size(),
                                0, nullptr, -PAD, trace.details->maxValue + PAD, ImVec2(0,100));
            }

            if( trace.flags & SHOW_GL_MATRICES )
            {
              ImGui::Columns(2, "GL State Header");
              ImGui::Separator();
              ImGui::Text("ModelView"); ImGui::NextColumn();
              ImGui::Text("Projection");
              ImGui::Separator();

              ImGui::Columns(2, "GL State");
              const std::array< float, 16 >& mv = trace.details->modelViewGL;
              ImGui::Text("%f\n%f\n%f\n%f\n", mv[0], mv[4], mv[8], mv[12]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", mv[1], mv[5], mv[9], mv[13]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", mv[2], mv[6], mv[10], mv[14]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", mv[3], mv[7], mv[11], mv[15]);

              ImGui::NextColumn();

              const std::array< float, 16 >& p = trace.details->projectionGL;
              ImGui::Text("%f\n%f\n%f\n%f\n", p[0], p[4], p[8], p[12]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", p[1], p[5], p[9], p[13]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", p[2], p[6], p[10], p[14]); ImGui::SameLine();
              ImGui::Text("%f\n%f\n%f\n%f\n", p[3], p[7], p[11], p[15]);
              ImGui::Columns(1);
            }

            ImGui::Separator();
         }
      }
      return isOpen;
   }
}

Profiler::Profiler( const char* name ) : _name( name )
{
   _traceStack.reserve( 20 );
   _traces.reserve( 20 );
}

void Profiler::draw()
{
   assert( _curTreeLevel == -1 && _traceStack.empty() ); // There is probably a popTrace missing.

   ImGui::SetNextWindowSize( ImVec2( 300, 100 ), ImGuiSetCond_Once );
   if ( !ImGui::Begin( _name ) )
   {
      ImGui::End();
      return;
   }

   // Draw the traces
   int level = 0;
   for ( size_t i = 0; i < _traces.size(); ++i )
   {
      auto& trace = _traces[i];
      if ( trace.level == level )
      {
         if ( drawTrace( trace ) )
         {
            ++level;
         }
      }
      else if ( trace.level < level )
      {
         ImGui::TreePop();
         if ( !drawTrace( trace ) )
         {
            --level;
         }
      }
   }

   while ( level-- > 0 )
   {
      ImGui::TreePop();
   }

   ImGui::End();
}

void Profiler::pushTrace( const char* traceName )
{
   // Increase the stack level
   ++_curTreeLevel;

   // Try to find if the trace already exists
   Trace trace {traceName, _curTreeLevel};
   auto it = std::lower_bound( _traces.begin(), _traces.end(), trace );
   if( it == _traces.end() || it->name != traceName )
   {
      // If it does not exists, append to the end of the vector, and flag
      // that a sort is required
      _traces.emplace_back( std::move( trace ) );
      it = _traces.end() - 1;
      _needSorting = true;
   }

   if( it->flags & SHOW_GL_MATRICES )
   {
      glGetFloatv (GL_MODELVIEW_MATRIX, it->details->modelViewGL.data()); 
      glGetFloatv (GL_PROJECTION_MATRIX, it->details->projectionGL.data()); 
   }

   // Add a new TracePushTime with the associated idx of the trace
   const size_t idx = std::distance( _traces.begin(), it );
   _traceStack.emplace_back( idx, std::chrono::system_clock::now() );
}

void Profiler::popTrace()
{
   assert( _traceStack.size() > 0 ); // push/pop count mismatch

   --_curTreeLevel;
   auto& trace = _traces[ _traceStack.back().first ];
   trace.details->prevTimes.push_back( trace.curTime );
   trace.details->maxValue = std::max( trace.curTime, trace.details->maxValue );
   trace.curTime =  static_cast<float>( std::chrono::duration_cast<std::chrono::nanoseconds>(
         ( std::chrono::system_clock::now() - _traceStack.back().second ) ).count() / 1000000.0f );
   _traceStack.pop_back();

   // Sort the traces if new traces were added. This should be done only when
   // the stack is empty since it keeps an index back to the traces vector and
   // we do not want to mess the ids.
   if( _needSorting && _traceStack.empty() )
   {
      std::sort( _traces.begin(), _traces.end() );
      _needSorting = false;
   }
}

Profiler::Trace::Trace( const char* aName, int aLevel ) :
  details{new TraceDetails()}, name{aName}, level{aLevel} 
{
}

} // end of namespace imdbg
