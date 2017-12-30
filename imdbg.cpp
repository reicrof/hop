#include "imdbg.h"
#include "imgui/imgui.h"

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

static void saveAsJson( const char* path, const vdbg::DisplayableTraceFrame& frame );

static inline bool EQUALS(float rhs, float lhs, float epsilon = 0.0005f) noexcept
{
    return (fabs(rhs - lhs) < epsilon);
}

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
   if( _recording )
   {
      _dispTraces.emplace_back( traces );
   }
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
   float maxFrameTime = _maxFrameTime;
   if( _allTimeMaxFrameTime >= 0.0f )
   {
      _allTimeMaxFrameTime =
          std::max( *std::max_element( values.begin(), values.end() ), _allTimeMaxFrameTime );
      maxFrameTime = _maxFrameTime = _allTimeMaxFrameTime;
   }

   int pickedFrame = ImGui::PlotHistogram(
       "",
       values.data(),
       values.size(),
       values.size() - 1,
       "Frames (ms)",
       0.001f,
       maxFrameTime * 1.05f,
       ImVec2{0, 100},
       sizeof( float ),
       values.size() - 1 );

   if ( pickedFrame != -1 )
   {
      _frameToShow -= ( _frameCountToShow - ( pickedFrame + 1 ) );
   }

   // Handle mousewheel
   if ( ImGui::IsItemHovered() )
   {
      ImGuiIO& io = ImGui::GetIO();
      if ( io.MouseWheel > 0 )
      {
         if ( io.KeyCtrl )
         {
            _maxFrameTime *= 0.95f;
            _allTimeMaxFrameTime = -1.0f;
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
            _allTimeMaxFrameTime = -1.0f;
         }
         else
         {
            if ( _frameCountToShow < _dispTraces.size() - 3 ) _frameCountToShow += 2;
         }
      }
   }


   ///////////////////////////
   ///////////////////////////

   _timeline.draw();

   // const float extraDrawPadding = 0; //128.0f;
   // const float windowWidthPxl = ImGui::GetWindowWidth() + extraDrawPadding;
   // static double stepSize = 1000;

   // static float startMillis = 3.0f;
   // ImGui::SliderFloat("Start millis", &startMillis, 0.0, 1000.0f, "%.4f", 1.1f);
   // static float millisToDisplay = 5.0f;

   // double startMicros = startMillis * 1000;
   // double microsToDisplay = millisToDisplay * 1000;

   // size_t stepsCount = microsToDisplay / stepSize;

   // constexpr double minStepSize = 10.0;
   // while( stepsCount > 140 || (stepsCount < 20 && stepSize > minStepSize) )
   // {
   //   if( stepsCount > 140 )
   //   {
   //      if( stepSize == minStepSize ) { stepSize = 8; }
   //      stepSize *= 5;
   //   }
   //   else if( stepsCount < 20 )
   //   {
   //      stepSize /= 5;
   //      stepSize = std::max( stepSize, minStepSize );
   //   }
   //   stepsCount = microsToDisplay / stepSize;
   // }
   // printf("%f micros\n", stepSize);

   // static double oldPos = 0;
   // static double newPos = 0;
   // const float prevMillis = millisToDisplay;
   // if( ImGui::SliderFloat("slider log float", &millisToDisplay, 0.1, 100000.0f, "%.4f", 1.5f) )
   // {
   //    const double microsToZoom = 6000;
   //    const double prevMicrosPerPxl = windowWidthPxl / (prevMillis*1000.0);
   //    double pxlPosition = (microsToZoom - (startMillis*1000.0)) * prevMicrosPerPxl;

   //    oldPos = pxlPosition;

   //    const double curMicrosPerPxl = windowWidthPxl / (millisToDisplay*1000.0);
   //    const double newPxlPos = (microsToZoom - (startMillis*1000.0)) * curMicrosPerPxl;

   //    newPos = newPxlPos;

   //    const auto timeDiff = (newPos - oldPos) / curMicrosPerPxl;

   //    startMillis += (timeDiff / 1000.0);
   // }

   //  const double microsPerPxl = windowWidthPxl / microsToDisplay;
   // const double stepSizePxl = microsPerPxl * stepSize;

   // ImVec2 top = ImGui::GetCursorScreenPos();
   // double modulus = fmod( startMicros, stepSize );
   // top.x -= (modulus * microsPerPxl);
   // if( startMicros * microsPerPxl > extraDrawPadding )
   // {
   //    top.x -= extraDrawPadding;
   // }
   // ImVec2 bottom = top;
   // bottom.y += 10;
   // ImDrawList* DrawList = ImGui::GetWindowDrawList();


   // auto testTop = top;
   // auto testBottom = bottom;
   // testTop.x += oldPos;
   // testBottom.x += oldPos;
   // testBottom.y += 30;
   // DrawList->AddLine( testTop, testBottom, ImGui::GetColorU32( ImGuiCol_ButtonActive ), 4.0f );

   // auto testTop2 = top;
   // auto testBottom2 = bottom;
   // testTop2.x += newPos;
   // testBottom2.x += newPos;
   // testBottom2.y += 30;
   // DrawList->AddLine( testTop2, testBottom2, ImGui::GetColorU32( ImGuiCol_HeaderActive ), 4.0f );

   // // auto testTop3 = top;
   // // auto testBottom3 = bottom;
   // // testTop3.x += (2600 * microsPerPxl) ;
   // // testBottom3.x += (2600 * microsPerPxl);
   // // testBottom3.y += 30;
   // // DrawList->AddLine( testTop3, testBottom3, ImGui::GetColorU32( ImGuiCol_ButtonActive ), 4.0f );

   // int count = (int)startMicros / stepSize;
   // std::vector< std::pair< ImVec2, double > > textPos;
   // for( double i = -extraDrawPadding + modulus; i < windowWidthPxl; i += stepSizePxl, ++count )
   // {
   //    if( count % 10 == 0 )
   //    {
   //       auto startEndLine = bottom;
   //       startEndLine.y += 10.0f;
   //       DrawList->AddLine( top, startEndLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //       textPos.emplace_back( startEndLine, count * stepSize );
   //    }
   //    else if( count % 5 == 0 )
   //    {
   //        auto midLine = bottom;
   //        midLine.y += 5.0f;
   //        DrawList->AddLine( top, midLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //    }
   //    else
   //    {
   //      DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f ); 
   //    }

   //    top.x += stepSizePxl;
   //    bottom.x += stepSizePxl;
   // }

   // const auto cursBackup = ImGui::GetCursorScreenPos();

   // const size_t total = stepsCount * stepSize;
   // if( total < 1000 )
   // {
   //      // print as microsecs
   //     for( const auto& pos : textPos )
   //     {
   //        ImGui::SetCursorScreenPos( pos.first );
   //        ImGui::Text( "%d us",  (int)pos.second );
   //     }
   // }
   // else if( total < 1000000 )
   // {
   //  // print as milliseconds
   //   for( const auto& pos : textPos )
   //   {
   //      ImGui::SetCursorScreenPos( pos.first );
   //      ImGui::Text( "%.3f ms", (pos.second / 1000) );
   //   }
   // }
   // else if( total < 1000000000 )
   // {
   //     // print as seconds
   //   for( const auto& pos : textPos )
   //   {
   //      ImGui::SetCursorScreenPos( pos.first );
   //      ImGui::Text( "%.3f s", (pos.second / 1000000) );
   //   }
   // }

   // ImGui::SetCursorScreenPos( cursBackup );

   ///////////////////////////
   ///////////////////////////

   // static float modifier = 1.0f;
   // ImGui::SliderFloat("slider log float", &secondToDisplay, 0.00001, 1000.0f, "%.4f", 2.0f);
   // float width = ImGui::GetWindowWidth();
   // float secsPerPxl = (secondToDisplay * width) / (modifier);
   // float sliceCount = width / secsPerPxl;

   // if( sliceCount < 5 )
   // {
   //    while( sliceCount < 5 )
   //    {
   //       modifier *= 5.0f;
   //       secsPerPxl = (secondToDisplay * width) / (modifier);
   //       sliceCount = width / secsPerPxl;
   //    }
   //    printf( "%f\n", modifier );
   // }
   // else if( sliceCount > 10 )
   // {
   //    while( sliceCount > 10 )
   //    {
   //       modifier /= 5.0f;
   //       secsPerPxl = (secondToDisplay * width) / (modifier);
   //       sliceCount = width / secsPerPxl;
   //    }
   //    printf( "%f\n", modifier );
   // }

   // ImVec2 top = ImGui::GetCursorScreenPos();
   // ImVec2 bottom = top;
   // bottom.y += 20;
   // ImDrawList* DrawList = ImGui::GetWindowDrawList();

   // std::vector< std::pair< ImVec2, double > > textPos;
   // for( float i = 0.0f; i < width; i += secsPerPxl )
   // {
   //    DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //    textPos.emplace_back( bottom, (i / secsPerPxl)*secondToDisplay );
   //    bottom.y -= 15;
   //    for( int i = 0; i < 4; ++i )
   //    {
   //       top.x += secsPerPxl/10.0f;
   //       bottom.x += secsPerPxl/10.0f;
   //       DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //    }
   //    top.x += secsPerPxl/10.0f;
   //    bottom.x += secsPerPxl/10.0f;
   //    ImVec2 midLine = bottom;
   //    midLine.y += 5.0f;
   //    DrawList->AddLine( top, midLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //    for( int i = 0; i < 5; ++i )
   //    {
   //       top.x += secsPerPxl/10.0f;
   //       bottom.x += secsPerPxl/10.0f;
   //       DrawList->AddLine( top, bottom, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 1.0f );
   //    }
   //    bottom.y += 15;
   // }
   // auto cursBackup = ImGui::GetCursorScreenPos();
   // for( const auto& pos : textPos )
   // {
   //    ImGui::SetCursorScreenPos( pos.first );
   //    ImGui::Text( "%f", pos.second );
   // }
   // ImGui::SetCursorScreenPos( cursBackup );

      //top = ImGui::GetCursorScreenPos();
      //top.x += i;
      //bottom.x = top.x;
   // top.y += 40;
   // ImGui::SetCursorScreenPos( top );
   ImGui::Button("Test", ImVec2(50, 20));
   //ImGui::EndChild();

   if( ImGui::DragFloat( "Max value", &_maxFrameTime, 0.005f ) )
      _allTimeMaxFrameTime = -1.0f;

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

static inline float msToPxl( float windowWidth, float msToDisplay, float ms )
{
   const float msPerPxl = msToDisplay / windowWidth;
   return ms / msPerPxl;
}

static inline float pxlToMs( float windowWidth, float msToDisplay, int pxl )
{
   const float msPerPxl = msToDisplay / windowWidth;
   return msPerPxl * pxl;
}

void vdbg::ProfilerTimeline::draw()
{
   constexpr float minStepSize = 0.01f;
   constexpr uint32_t minStepCount = 20;
   constexpr uint32_t maxStepCount = 140;

   const auto cursBackup = ImGui::GetCursorScreenPos();
   const float windowWidthPxl = ImGui::GetWindowWidth()-32;

   const float prevMillis = _millisToDisplay;
   if( ImGui::SliderFloat("slider log float", &_millisToDisplay, 0.1, 100000.0f, "%.4f", 1.5f) )
   {
      const float msToZoom = 6.0 - _startMillis;
      const float prevPxlPos = msToPxl( windowWidthPxl, prevMillis, msToZoom );
      const float newPxlPos = msToPxl( windowWidthPxl, _millisToDisplay, msToZoom );

      const float pxlDiff = newPxlPos - prevPxlPos;
      if( ! EQUALS(pxlDiff, 0.0f) )
      {
         const float timeDiff = pxlToMs( windowWidthPxl, _millisToDisplay, pxlDiff );
         printf("%f pxlDiff = %f from %f - %f\n", timeDiff, pxlDiff, newPxlPos, prevPxlPos );
         _startMillis += timeDiff;
      }
   }

   ImGui::SliderFloat("Start millis", &_startMillis, -100.0, 100000.0f, "%.1f" );

   const float stepsCount = [this, minStepSize]()
   {
      float stepsCount = _millisToDisplay / _stepSizeInMillis;
      while( stepsCount > maxStepCount || (stepsCount < minStepCount && _stepSizeInMillis > minStepSize) )
      {
        if( stepsCount > maxStepCount )
        {
           if( _stepSizeInMillis == minStepSize ) { _stepSizeInMillis = 0.008f; }
           _stepSizeInMillis *= 5.0f;
        }
        else if( stepsCount < minStepCount )
        {
           _stepSizeInMillis /= 5.0f;
           _stepSizeInMillis = std::max( _stepSizeInMillis, minStepSize );
        }
        stepsCount = _millisToDisplay / _stepSizeInMillis;
      }
      return stepsCount;
   }();

   // Start drawing the lines on the timeline
   constexpr float smallLineLength = 10.0f;
   constexpr float deltaBigLineLength = 12.0f; // The diff between the small line and big one
   constexpr float deltaMidLineLength = 7.0f; // The diff between the small line and mid one
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const float stepSizePxl = msToPxl( windowWidthPxl, _millisToDisplay, _stepSizeInMillis );
   const float stepsDone = _startMillis / _stepSizeInMillis;
   const double remainder = fmod( (double)_startMillis, (double)_stepSizeInMillis );
   float remainderPxl = 0.0f;
   if( !EQUALS( remainder, 0.0f ) && !EQUALS( remainder, _stepSizeInMillis ) )
   {
      remainderPxl = msToPxl( windowWidthPxl, _millisToDisplay, remainder );
   }

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top = ImGui::GetCursorScreenPos();
   top.x -= (stepSizePxl + remainderPxl) - stepSizePxl ;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int count = stepsDone;
   std::vector< std::pair< ImVec2, double > > textPos;
   for( double i = top.x; i < (windowWidthPxl + stepSizePxl); i += stepSizePxl, ++count )
   {
      // Draw biggest begin/end lines
      auto test = fmod( stepsDone, _stepSizeInMillis );
      (void)test;
      if( count % 10 == 0 )
      {
         auto startEndLine = bottom;
         startEndLine.y += deltaBigLineLength;
         DrawList->AddLine( top, startEndLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 2.0f );
         textPos.emplace_back( startEndLine, count * _stepSizeInMillis );
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

   const float total = stepsCount * _stepSizeInMillis;
   if( total < 1.0f )
   {
        // print as microsecs
       for( const auto& pos : textPos )
       {
          ImGui::SetCursorScreenPos( pos.first );
          ImGui::Text( "%d us", (int)(pos.second / 1000) );
       }
   }
   else if( total < 1000.0f )
   {
    // print as milliseconds
     for( const auto& pos : textPos )
     {
        ImGui::SetCursorScreenPos( pos.first );
        ImGui::Text( "%.3f ms", pos.second );
     }
   }
   else if( total < 1000000.0f )
   {
       // print as seconds
     for( const auto& pos : textPos )
     {
        ImGui::SetCursorScreenPos( pos.first );
        ImGui::Text( "%.3f s", (pos.second / 1000) );
     }
   }

   auto tmpBkcup = cursBackup;
   tmpBkcup.y += 80;
   ImGui::SetCursorScreenPos( tmpBkcup );
}