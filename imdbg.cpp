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

//static void saveAsJson( const char* path, const vdbg::DisplayableTrace& frame );

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
                                           // Mouse callbacks
   // glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
   // glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
}

void addNewProfiler( Profiler* profiler )
{
   _profilers.push_back( profiler );
}

Profiler::Profiler( const std::string& name ) : _name( name )
{
}

// void Profiler::draw()
// {
//    ImGui::SetNextWindowSize(ImVec2(700,500), ImGuiSetCond_FirstUseEver);
//    if ( !ImGui::Begin( _name.c_str() ) )
//    {
//       // Early out
//       ImGui::End();
//       return;
//    }

//    for( size_t i = 0; i < _threadsId.size(); ++i )
//    {
//       std::string headerName( "Thread " + std::to_string( _threadsId[i] ) );
//       if( ImGui::CollapsingHeader( headerName.c_str() ) )
//       {
//          auto& threadTrace = _tracesPerThread[i];
//          ImGui::PushID( &threadTrace );
//          //threadTrace.draw();
//          ImGui::PopID();
//          ImGui::Spacing();
//          ImGui::Spacing();
//          ImGui::Spacing();
//       }
//    }

//    ImGui::End();
// }

void Profiler::addTraces( const std::vector<DisplayableTrace>& traces, uint32_t threadId )
{
   if ( _recording )
   {
      // The traces should already be sorted.
      assert( std::is_sorted( traces.begin(), traces.end() ) );

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
         _stringDataPerThread.emplace_back();
      }

      // Insert the new traces in the list and keep the previous size around
      const auto prevSize = _tracesPerThread[i].size();
      _tracesPerThread[i].insert( _tracesPerThread[i].end(), traces.begin(), traces.end() );

      // The first trace added has the smallest time (since they are already sorted).
      // Thus all the traces with a bigger time than that could now be in an unsorted state.
      // We need to sort all the traces that have a time >= to this trace
      auto it = std::lower_bound(
          _tracesPerThread[i].begin(), _tracesPerThread[i].begin() + prevSize, traces.front() );
      std::sort( it, _tracesPerThread[i].end() );
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
         _stringDataPerThread.emplace_back();
      }

      _stringDataPerThread[i].insert( _stringDataPerThread[i].end(), strData.begin(), strData.end() );
   }
}

} // end of namespace vdbg

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

// static void saveAsJson( const char* path, const vdbg::DisplayableTraceFrame& frame )
// {
//    using namespace rapidjson;

//    StringBuffer s;
//    PrettyWriter<StringBuffer> writer(s);
//    printf( "Should save as json\n" );
//    writer.StartObject();
//    writer.Key("traceEvents");
//    writer.StartArray();
//    const auto& traces = frame.traces;
//    const uint32_t threadId = frame.threadId;
//    for( const auto& t : traces )
//    {
//       writer.StartObject();
//       writer.Key("ts");
//       writer.Double( (double)t.time );
//       writer.Key("ph");
//       writer.String( t.flags & vdbg::DisplayableTrace::START_TRACE ? "B" : "E" );
//       writer.Key("pid");
//       writer.Uint(threadId);
//       //writer.Key("name");
//       //writer.String( t.name );
//       assert( false && "should create name array here" );
//       writer.EndObject();
//    }
//    writer.EndArray();
//    writer.Key("displayTimeUnit");
//    writer.String("ms");
//    writer.EndObject();

//    std::ofstream of(path);
//    of << s.GetString();

// }

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

void vdbg::Profiler::draw()
{
   ImGui::SetNextWindowSize(ImVec2(700,500), ImGuiSetCond_FirstUseEver);
   if ( !ImGui::Begin( _name.c_str(), nullptr, ImGuiWindowFlags_NoScrollWithMouse ) )
   {
      // Early out
      ImGui::End();
      return;
   }

   ImGui::Checkbox( "Listening", &_recording );
   ImGui::SameLine();
   ImGui::Checkbox( "Live", &_realtime );

   // if ( _realTime && _recording )
   // {
   //    _frameToShow = std::max( ( (int)_dispTraces.size() ) - 1, 0 );
   // }

   //ImGui::SliderInt( "Frame to show", &_frameToShow, 0, _dispTraces.size() - 1 );

   // std::vector<float> values( _frameCountToShow, 0.0f );
   // if ( !_dispTraces.empty() )
   // {
   //    int startFrame = std::max( (int)_frameToShow - (int)( _frameCountToShow - 1 ), 0 );
   //    int count = 0;
   //    for ( int i = startFrame + 1; i <= (int)_frameToShow; ++i, ++count )
   //    {
   //       values[count] = _dispTraces[i].traces.front().deltaTime;
   //    }
   // }

   // int offset = _frameCountToShow;
   // if( offset > (int)values.size() )
   //    offset = 0;
   // float maxFrameTime = _maxFrameTime;
   // if( _allTimeMaxFrameTime >= 0.0f )
   // {
   //    _allTimeMaxFrameTime =
   //        std::max( *std::max_element( values.begin(), values.end() ), _allTimeMaxFrameTime );
   //    maxFrameTime = _maxFrameTime = _allTimeMaxFrameTime;
   // }

   // int pickedFrame = ImGui::PlotHistogram(
   //     "",
   //     values.data(),
   //     values.size(),
   //     values.size() - 1,
   //     "Frames (ms)",
   //     0.001f,
   //     maxFrameTime * 1.05f,
   //     ImVec2{0, 100},
   //     sizeof( float ),
   //     values.size() - 1 );

   // if ( pickedFrame != -1 )
   // {
   //    _frameToShow -= ( _frameCountToShow - ( pickedFrame + 1 ) );
   // }

   // Handle mousewheel
   // if ( ImGui::IsItemHovered() )
   // {
   //    ImGuiIO& io = ImGui::GetIO();
   //    if ( io.MouseWheel > 0 )
   //    {
   //       if ( io.KeyCtrl )
   //       {
   //          _maxFrameTime *= 0.95f;
   //          _allTimeMaxFrameTime = -1.0f;
   //       }
   //       else
   //       {
   //          if ( _frameCountToShow > 6 ) _frameCountToShow -= 2;
   //       }
   //    }
   //    else if ( io.MouseWheel < 0 )
   //    {
   //       if ( io.KeyCtrl )
   //       {
   //          _maxFrameTime *= 1.05f;
   //          _allTimeMaxFrameTime = -1.0f;
   //       }
   //       else
   //       {
   //          if ( _frameCountToShow < _dispTraces.size() - 3 ) _frameCountToShow += 2;
   //       }
   //    }
   // }

   {
      // Move timeline to the most recent trace if Live mode is on
      if( _realtime && _recording )
      {
         // Only valid if we have at least 1 thread + 1 trace
         if( !_tracesPerThread.empty() && !_tracesPerThread[0].empty() )
         {
            const auto relativeStart = _tracesPerThread[0].front().time;
            double maxTime = 0.0;
            for( const auto& t : _tracesPerThread )
            {
               if( !t.empty() )
               {
                  maxTime = std::max( maxTime, t.back().time );
               }
            }
            _timeline.moveToTime( (maxTime - relativeStart) / 1000 );
         }
      }

      ImGui::BeginGroup();
      const auto canvasPos = ImGui::GetCursorScreenPos();
      _timeline.drawTimeline();

      for( size_t i = 0; i < _tracesPerThread.size(); ++i )
      {
         _timeline.drawTraces( _tracesPerThread[i], _stringDataPerThread[i] );
         ImGui::InvisibleButton("trace-padding", ImVec2( 20, 40 ) );
      }

      ImVec2 mousePosInCanvas = ImVec2(ImGui::GetIO().MousePos.x - canvasPos.x, ImGui::GetIO().MousePos.y - canvasPos.y);
      ImGui::EndGroup();
      _timeline.handleMouseWheel( mousePosInCanvas );
   }

   ImGui::InvisibleButton("padding", ImVec2( 20, 40 ) );

   // if( ImGui::DragFloat( "Max value", &_maxFrameTime, 0.005f ) )
   //    _allTimeMaxFrameTime = -1.0f;

   // // Draw the traces
   // if ( !_dispTraces.empty() )
   // {
   //    const vdbg::DisplayableTraceFrame& frameToDraw = _dispTraces[_frameToShow];
   //    for ( size_t i = 0; i < frameToDraw.traces.size(); )
   //    {
   //       if ( !drawDispTrace( frameToDraw, i ) )
   //       {
   //          ++i;
   //       }
   //    }
   // }

   ImGui::Spacing();

   static char pathToSave[256] = "";
   ImGui::InputText( "", pathToSave, 256 );
   ImGui::SameLine();
   if ( ImGui::Button( "Save As" ) )
   {
      // if ( _frameCountToShow < _dispTraces.size() )
      // {
      //    //saveAsJson( pathToSave, _dispTraces[_frameToShow] );
      // }
   }

   ImGui::End();
}

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

void vdbg::ProfilerTimeline::drawTimeline()
{
   constexpr int64_t minStepSize = 10;
   constexpr int64_t minStepCount = 20;
   constexpr int64_t maxStepCount = 140;

   const auto canvasPos = ImGui::GetCursorScreenPos();
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

   // Start drawing the lines on the timeline
   constexpr float smallLineLength = 10.0f;
   constexpr float deltaBigLineLength = 12.0f; // The diff between the small line and big one
   constexpr float deltaMidLineLength = 7.0f; // The diff between the small line and mid one
   ImDrawList* DrawList = ImGui::GetWindowDrawList();

   const int64_t stepSizePxl = microsToPxl( windowWidthPxl, _microsToDisplay, _stepSizeInMicros );
   const int64_t stepsDone = _startMicros / _stepSizeInMicros;
   const int64_t remainder = _startMicros % _stepSizeInMicros;
   int remainderPxl = 0;
   if( remainder != 0 )
      remainderPxl = microsToPxl( windowWidthPxl, _microsToDisplay, remainder );

   // Start drawing one step before the start position to account for partial steps
   ImVec2 top = ImGui::GetCursorScreenPos();
   top.x -= (stepSizePxl + remainderPxl) - stepSizePxl ;
   ImVec2 bottom = top;
   bottom.y += smallLineLength;

   int count = stepsDone;
   std::vector< std::pair< ImVec2, double > > textPos;
   const auto maxPosX = canvasPos.x + windowWidthPxl;
   for( double i = top.x; i < maxPosX; i += stepSizePxl, ++count )
   {
      // Draw biggest begin/end lines
      if( count % 10 == 0 )
      {
         auto startEndLine = bottom;
         startEndLine.y += deltaBigLineLength;
         DrawList->AddLine( top, startEndLine, ImGui::GetColorU32( ImGuiCol_TextDisabled ), 3.0f );
         textPos.emplace_back( startEndLine, count * _stepSizeInMicros );
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

   auto tmpBkcup = canvasPos;
   tmpBkcup.y += 80;
   ImGui::SetCursorScreenPos( tmpBkcup );
}

void vdbg::ProfilerTimeline::handleMouseWheel( const ImVec2& mousePosInCanvas )
{
   if ( ImGui::IsItemHovered() )
   {
      const float windowWidthPxl = ImGui::GetWindowWidth();
      ImGuiIO& io = ImGui::GetIO();
      if ( io.MouseWheel > 0 )
      {
         if ( io.KeyCtrl )
         {
            zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosInCanvas.x ) + _startMicros, 0.9f );
         }
         else
         {
             _startMicros += 0.05f * _microsToDisplay;
         }
      }
      else if ( io.MouseWheel < 0 )
      {
         if ( io.KeyCtrl )
         {
            zoomOn( pxlToMicros( windowWidthPxl, _microsToDisplay, mousePosInCanvas.x ) + _startMicros , 1.1f );
         }
         else
         {
            _startMicros -= 0.05f * _microsToDisplay;
         }
      }
   }
}

void vdbg::ProfilerTimeline::moveToTime( int64_t timeInMicro )
{
   _startMicros = timeInMicro - (_microsToDisplay / 2);
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

void vdbg::ProfilerTimeline::drawTraces( const std::vector<DisplayableTrace>& traces, const std::vector< char >& strData )
{
   if( traces.empty() ) return;

   const auto relativeStart = traces[0].time;
   const float windowWidthPxl = ImGui::GetWindowWidth();
   const auto startMicrosAsPxl = microsToPxl( windowWidthPxl, _microsToDisplay, _startMicros );
   const auto canvasPos = ImGui::GetCursorScreenPos();

   std::vector< ImVec2 > pos;
   std::vector< float > length;
   std::vector< uint32_t > fctName, className;
   pos.reserve( 50 );
   fctName.reserve( 50 );
   className.reserve( 50 );

   const double firstTraceAbsoluteTime = relativeStart + (_startMicros * 1000.0);
   const double lastTraceAbsoluteTime = firstTraceAbsoluteTime + ( _microsToDisplay * 1000.0 );
   int curLevel = -1;
   int maxLevel = 0;
   for ( const auto& t : traces )
   {
      if ( t.isStartTrace() )
      {
         ++curLevel;
         if ( t.time > firstTraceAbsoluteTime ||
              t.time + ( t.deltaTime * 1000.0 ) > firstTraceAbsoluteTime )
         {
            // The start time is bigger than the maximum time on the timeline. We are done
            // drawing the traces.
            if ( t.time > lastTraceAbsoluteTime ) break;

            const int64_t traceStartInMicros = ((t.time - relativeStart) / 1000.0f) + 0.5f;
            maxLevel = std::max( curLevel, maxLevel );
            const auto traceStartPxl =
                microsToPxl<float>( windowWidthPxl, _microsToDisplay, traceStartInMicros );
            const float traceLengthPxl =
                microsToPxl<float>( windowWidthPxl, _microsToDisplay, t.deltaTime );

            // Skip trace if it is way smaller than treshold
            if( traceLengthPxl < 0.25f )
                  continue;

            pos.push_back( ImVec2(
                canvasPos.x - startMicrosAsPxl + traceStartPxl, canvasPos.y + curLevel * 22.0f ) );
            length.push_back( traceLengthPxl );
            fctName.push_back( t.fctNameIndex );
            className.push_back( t.classNameIndex );
         }
      }
      else
      {
         --curLevel;
      }
   }

   char curName[ 512 ] = {};
   for( size_t i = 0; i < pos.size(); ++i )
   {
      if( className[i] > 0 )
      {
         strncpy( curName, &strData[ className[i] ], sizeof( curName )-1 );
         strncat( curName, "::", sizeof( curName ) - strlen( curName ) -1 );
      }
      strncat( curName, &strData[ fctName[i] ], sizeof( curName ) - strlen( curName ) -1 );

      ImGui::SetCursorScreenPos( pos[i] );
      ImGui::Button( curName, ImVec2(length[i],20) );
      curName[0] = '\0';
   }

   auto newPos = canvasPos;
   newPos.y += maxLevel * 22.0f;
   ImGui::SetCursorScreenPos( newPos );
}