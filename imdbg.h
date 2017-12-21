#ifndef IMDBG_H_
#define IMDBG_H_

#include "imgui/imgui.h"
#include <message.h>
#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace vdbg
{
   struct DisplayableTrace
   {
      double time;
      float deltaTime;
      uint32_t flags;
      char name[64];
   };
   struct DisplayableTraceFrame
   {
      uint32_t threadId;
      std::vector< DisplayableTrace > traces;
   };
    struct ThreadTraces
    {
       void draw();
       void addTraces( const vdbg::DisplayableTraceFrame& traces );

       std::vector< vdbg::DisplayableTraceFrame > _dispTraces;
       int _frameToShow{0};
       float _maxFrameTime{0.1f};
       float _allTimeMaxFrameTime{0.1f};
       size_t _frameCountToShow{50};
       bool _recording{false};
       bool _realTime{true};
    };
}

namespace imdbg
{
    // Initialize the imgui framework
    void init();
    // Updates the imgui data. Should be called each frame
    void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
    // Draw the ui
    void draw();

    // New profiler window
    class Profiler
    {
    public:
       Profiler( Profiler&& ) = default;
       Profiler( const Profiler& ) = delete;
       Profiler& operator=( const Profiler& ) = delete;
       Profiler& operator=( Profiler&& ) = delete;

       void draw();
       void pushTrace( const char* traceName );
       void popTrace();

       void addTraces( const vdbg::DisplayableTraceFrame& traces );


      private:
       friend Profiler* newProfiler(const std::string& name );
       Profiler(const std::string& name );

       std::string _name;

       bool _recording{true};
       std::vector< uint32_t > _threadsId;
       std::vector< vdbg::ThreadTraces > _tracesPerThread;
    };

    // Returns a non-owning pointer of a new profiler.
    Profiler* newProfiler( const std::string& name );
}

#endif  // IMDBG_H_
