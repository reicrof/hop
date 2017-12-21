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
      TimeStamp time;
      float deltaTime;
      uint32_t flags;
      char name[64];
   };
   struct DisplayableTraceFrame
   {
      uint32_t threadId;
      std::vector< DisplayableTrace > traces;
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
       struct TraceDetails
       {
          // Gl state values
          std::array< float, 16 > modelViewGL;
          std::array< float, 16 > projectionGL;

          // Previous trace time values
          std::vector< float > prevTimes;
          float maxValue { 0.0f };
       };

       struct Trace
       {
          Trace( const char* aName, int aLevel );
          std::unique_ptr< TraceDetails > details; // Cold data
          const char* name{nullptr};
          float curTime;
          int level{-1};
          unsigned flags {0};
          bool operator<( const Trace& rhs ) const { return name < rhs.name; }
       };
       using TracePushTime = 
          std::pair< size_t, /*idx in vector of the trace*/
                     std::chrono::time_point<std::chrono::system_clock> >;

       Profiler( Profiler&& ) = default;
       Profiler( const Profiler& ) = delete;
       Profiler& operator=( const Profiler& ) = delete;
       Profiler& operator=( Profiler&& ) = delete;

       void draw();
       void pushTrace( const char* traceName );
       void popTrace();

       uint32_t getThreadId() const { return _threadId;}
       void setThreadId( uint32_t t )
       {
            _threadId = t;
       }

       void addTraces( const vdbg::DisplayableTraceFrame& traces );


      private:
       friend Profiler* newProfiler(const std::string& name );
       Profiler(const std::string& name );

       std::vector<Trace> _traces;
       std::vector<TracePushTime> _traceStack;

       std::vector< vdbg::DisplayableTraceFrame > _dispTraces;
       std::string _name;
       size_t  _traceCount{ 0 };
       int _curTreeLevel{ -1 };
       bool _needSorting{ false };

       uint32_t _threadId{0};
       bool _recording{false};
       bool _realTime{true};
       int _frameToShow{0};
       float _maxFrameTime{2.0f};
       size_t _frameCountToShow{50};
    };

    // Returns a non-owning pointer of a new profiler.
    Profiler* newProfiler( const std::string& name );
}

#endif  // IMDBG_H_
