#ifndef IMDBG_H_
#define IMDBG_H_

#include "Hop.h"
#include "ThreadInfo.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace hop
{
class ProfilerTimeline
{
public:
   void draw(
       const std::vector<ThreadInfo>& _tracesPerThread,
       const std::vector<uint32_t>& threadIds );
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absolutePresentTime() const noexcept;
   void setAbsoluteStartTime( TimeStamp time ) noexcept;
   void setAbsolutePresentTime( TimeStamp time ) noexcept;
   int64_t microsToDisplay() const noexcept;
   float windowWidthPxl() const noexcept;

   void moveToStart() noexcept;
   void moveToPresentTime() noexcept;
   void moveToTime( int64_t timeInMicro ) noexcept;

   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

private:
   void drawTimeline( const float posX, const float posY );
   void drawTraces( const ThreadInfo& traces, int threadIndex, const float posX, const float posY );
   void drawLockWaits( const ThreadInfo& traces, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );

   static constexpr float TRACE_HEIGHT = 20.0f;
   static constexpr float TRACE_VERTICAL_PADDING = 2.0f;

   int64_t _startMicros{0};
   uint64_t _microsToDisplay{50000};
   int64_t _stepSizeInMicros{1000};
   TimeStamp _absoluteStartTime{};
   TimeStamp _absolutePresentTime{};
   float _rightClickStartPosInCanvas[2] = {};
   TDepth_t _maxTraceDepthPerThread[ MAX_THREAD_NB ] = {};
   float _windowWidthPxl{0};
   bool _realtime{true};
};

class Server;
struct Profiler
{
   Profiler( const std::string& name );
   ~Profiler();
   void draw();
   void fetchClientData();
   void addTraces( const DisplayableTraces& traces, uint32_t threadId );
   void addStringData( const std::vector< char >& stringData, uint32_t threadId );
   void addLockWaits( const std::vector< LockWait >& lockWaits, uint32_t threadId );
   void handleHotkey();
   void setRecording( bool recording );

private:
   void drawMenuBar();

   std::string _name;
   ProfilerTimeline _timeline;
   std::vector< uint32_t > _threadsId;
   std::vector< ThreadInfo > _tracesPerThread;
   bool _recording{ false };

   // Client/Server data
   // TODO: rethink and redo this part
   std::unique_ptr< Server > _server;
   std::vector< uint32_t > threadIds;
   std::vector< hop::DisplayableTraces > pendingTraces;
   std::vector< std::vector< char > > stringData;

   std::vector< uint32_t > threadIdsLockWaits;
   std::vector<std::vector< hop::LockWait > > pendingLockWaits;
};

// Initialize the imgui framework
void init();
// Add new profiler to be drawn
void addNewProfiler( Profiler* profiler );
// Updates the imgui data. Should be called each frame
void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
// Draw the ui
void draw();

} // namespace hop

#endif  // IMDBG_H_
