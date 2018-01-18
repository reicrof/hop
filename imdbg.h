#ifndef IMDBG_H_
#define IMDBG_H_

#include <vdbg.h>
#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace vdbg
{
struct DisplayableTraces
{
   // The ends order specify the order of the traces
   std::vector< TimeStamp > ends; // in ns

   // Since the starts time may not have the same order as
   // the ends, they have been resorted (for faster lookup)
   // and also provides the index of the trace they are associated
   std::vector< std::pair< TimeStamp, size_t > > starts; // in ns

   //Indexes of the name in the string database
   std::vector< TStrIdx_t > fileNameIds;
   std::vector< TStrIdx_t > classNameIds;
   std::vector< TStrIdx_t > fctNameIds;
   std::vector< TLineNb_t > lineNbs;
   std::vector< uint32_t > flags;

   enum Flags
   {
      END_TRACE = 0,
      START_TRACE = 1,
   };

   void reserve( size_t size )
   {
      starts.reserve( size );
      ends.reserve( size );
      flags.reserve( size );
      fileNameIds.reserve( size );
      classNameIds.reserve( size );
      fctNameIds.reserve( size );
      lineNbs.reserve( size );
   }

   void clear()
   {
      starts.clear();
      ends.clear();
      flags.clear();
      fileNameIds.clear();
      classNameIds.clear();
      fctNameIds.clear();
      lineNbs.clear();
   }

   // inline bool isStartTrace() const noexcept
   // {
   //    return flags & START_TRACE;
   // }

   // inline friend bool operator<( const DisplayableTrace& lhs, const DisplayableTrace& rhs )
   // {
   //    return lhs.time < rhs.time;
   // }
};

struct ThreadTraces
{
   static constexpr int CHUNK_SIZE = 2048;
   ThreadTraces();
   void addTraces( const DisplayableTraces& traces );
   void addLockWaits( const std::vector< LockWait >& lockWaits );
   std::vector< TimeStamp > startTimes;
   std::vector< TimeStamp > endTimes;
   DisplayableTraces traces;
   std::vector< char > stringData;
   std::vector< LockWait > _lockWaits;
};

class ProfilerTimeline
{
public:
   void draw(
       const std::vector<ThreadTraces>& _tracesPerThread,
       const std::vector<uint32_t>& threadIds );
   TimeStamp absoluteStartTime() const noexcept;
   TimeStamp absolutePresentTime() const noexcept;
   void setAbsoluteStartTime( TimeStamp time ) noexcept;
   void setAbsolutePresentTime( TimeStamp time ) noexcept;

   void moveToStart() noexcept;
   void moveToPresentTime() noexcept;
   void moveToTime( int64_t timeInMicro ) noexcept;

   void setRealtime( bool isRealtime ) noexcept;
   bool realtime() const noexcept;

private:
   void drawTimeline( const float posX, const float posY );
   void drawTraces( const ThreadTraces& traces, int threadIndex, const float posX, const float posY );
   void drawLockWaits( const ThreadTraces& traces, const float posX, const float posY );
   void handleMouseDrag( float mousePosX, float mousePosY );
   void handleMouseWheel( float mousePosX, float mousePosY );
   void zoomOn( int64_t microToZoomOn, float zoomFactor );

   static constexpr float TRACE_HEIGHT = 20.0f;
   static constexpr float TRACE_VERTICAL_PADDING = 2.0f;

   int64_t _startMicros{0};
   int64_t _microsToDisplay{50000};
   int64_t _stepSizeInMicros{1000};
   TimeStamp _absoluteStartTime{};
   TimeStamp _absolutePresentTime{};
   float _rightClickStartPosInCanvas[2] = {};
   int _maxTraceDepthPerThread[ MAX_THREAD_NB ] = {};
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

private:
   void drawMenuBar();

   std::string _name;
   ProfilerTimeline _timeline;
   std::vector< uint32_t > _threadsId;
   std::vector< ThreadTraces > _tracesPerThread;
   bool _recording{ false };

   // Client/Server data
   // TODO: rethink and redo this part
   std::unique_ptr< Server > _server;
   std::vector< uint32_t > threadIds;
   std::vector< vdbg::DisplayableTraces > pendingTraces;
   std::vector< std::vector< char > > stringData;

   std::vector< uint32_t > threadIdsLockWaits;
   std::vector<std::vector< vdbg::LockWait > > pendingLockWaits;
};

// Initialize the imgui framework
void init();
// Add new profiler to be drawn
void addNewProfiler( Profiler* profiler );
// Updates the imgui data. Should be called each frame
void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
// Draw the ui
void draw();

} // namespace vdbg

#endif  // IMDBG_H_
