#ifndef IMDBG_H_
#define IMDBG_H_

#include "imgui/imgui.h"
#include <vdbg.h>
#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace vdbg
{
struct DisplayableTrace
{
   TimeStamp time; // in ns
   uint32_t deltaTime; // in ns
   uint32_t flags;
   // Indexes of the name in the string database
   uint32_t classNameIndex;
   uint32_t fctNameIndex;

   enum Flags
   {
      END_TRACE = 0,
      START_TRACE = 1,
   };

   inline bool isStartTrace() const noexcept
   {
      return flags & START_TRACE;
   }

   inline friend bool operator<( const DisplayableTrace& lhs, const DisplayableTrace& rhs )
   {
      return lhs.time < rhs.time;
   }
};

struct ThreadTraces
{
   static constexpr int CHUNK_SIZE = 2048;
   ThreadTraces();
   void addTraces( const std::vector< DisplayableTrace >& traces );
   std::vector< TimeStamp > startTimes;
   std::vector< TimeStamp > endTimes;
   std::vector< std::vector< DisplayableTrace > > chunks;
};

struct ProfilerTimeline
{
   void drawTimeline();
   void drawTraces( const ThreadTraces& traces, const std::vector< char >& strData );
   void handleMouseWheel( const ImVec2& mousePosInCanvas );
   void moveToTime( int64_t timeInMicro );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );
   int64_t _startMicros{3000};
   int64_t _microsToDisplay{5000};
   int64_t _stepSizeInMicros{1000};
   int _maxTracesDepth{0};
};

class Server;
struct Profiler
{
   Profiler( const std::string& name );
   void draw( Server* server );
   void addTraces( const std::vector< DisplayableTrace >& traces, uint32_t threadId );
   void addStringData( const std::vector< char >& stringData, uint32_t threadId );

   std::string _name;
   ProfilerTimeline _timeline;
   std::vector< uint32_t > _threadsId;
   //std::vector< std::vector< DisplayableTrace > > _tracesPerThread;
   std::vector< ThreadTraces > _tracesPerThread;
   std::vector< std::vector< char > > _stringDataPerThread;
   bool _recording{ false };
   bool _realtime{ true };
};

// Initialize the imgui framework
void init();
// Add new profiler to be drawn
void addNewProfiler( Profiler* profiler );
// Updates the imgui data. Should be called each frame
void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
// Draw the ui
void draw( Server* server );

} // namespace vdbg

#endif  // IMDBG_H_
