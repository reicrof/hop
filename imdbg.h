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
   DisplayableTraces() = default;
   DisplayableTraces(DisplayableTraces&& ) = default;
   DisplayableTraces(const DisplayableTraces& ) = delete;
   DisplayableTraces& operator=(const DisplayableTraces& ) = delete;

   enum Flags
   {
      END_TRACE = 0,
      START_TRACE = 1,
   };

   void append( const DisplayableTraces& newTraces )
   {
      deltas.insert( deltas.end(), newTraces.deltas.begin(), newTraces.deltas.end() );
      ends.insert( ends.end(), newTraces.ends.begin(), newTraces.ends.end() );
      flags.insert( flags.end(), newTraces.flags.begin(), newTraces.flags.end() );
      fileNameIds.insert( fileNameIds.end(), newTraces.fileNameIds.begin(), newTraces.fileNameIds.end() );
      classNameIds.insert( classNameIds.end(), newTraces.classNameIds.begin(), newTraces.classNameIds.end() );
      fctNameIds.insert( fctNameIds.end(), newTraces.fctNameIds.begin(), newTraces.fctNameIds.end() );
      lineNbs.insert( lineNbs.end(), newTraces.lineNbs.begin(), newTraces.lineNbs.end() );
      depths.insert( depths.end(), newTraces.depths.begin(), newTraces.depths.end() );
   }

   void reserve( size_t size )
   {
      ends.reserve( size );
      deltas.reserve( size );
      flags.reserve( size );
      fileNameIds.reserve( size );
      classNameIds.reserve( size );
      fctNameIds.reserve( size );
      lineNbs.reserve( size );
      depths.reserve( size );
   }

   void clear()
   {
      ends.clear();
      deltas.clear();
      flags.clear();
      fileNameIds.clear();
      classNameIds.clear();
      fctNameIds.clear();
      lineNbs.clear();
      depths.clear();
   }

   // The ends order specify the order of the traces
   std::vector< TimeStamp > ends; // in ns
   std::vector< TimeStamp > deltas; // in ns

   //Indexes of the name in the string database
   std::vector< TStrIdx_t > fileNameIds;
   std::vector< TStrIdx_t > classNameIds;
   std::vector< TStrIdx_t > fctNameIds;

   std::vector< TLineNb_t > lineNbs;
   std::vector< TDepth_t > depths;
   std::vector< uint32_t > flags;
};

struct ThreadData
{
   static constexpr int CHUNK_SIZE = 2048;
   ThreadData();
   void addTraces( const DisplayableTraces& traces );
   void addLockWaits( const std::vector< LockWait >& lockWaits );
   DisplayableTraces traces;
   std::vector< char > stringData;
   std::vector< LockWait > _lockWaits;
};

class ProfilerTimeline
{
public:
   void draw(
       const std::vector<ThreadData>& _tracesPerThread,
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
   void drawTraces( const ThreadData& traces, int threadIndex, const float posX, const float posY );
   void drawLockWaits( const ThreadData& traces, const float posX, const float posY );
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
   TDepth_t _maxTraceDepthPerThread[ MAX_THREAD_NB ] = {};
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
   std::vector< ThreadData > _tracesPerThread;
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
