#ifndef PROFILER_H_
#define PROFILER_H_

#include "Server.h"
#include "Timeline.h"
#include "TimelineTrack.h"
#include "TimelineStats.h"
#include "TraceSearch.h"
#include "StringDb.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace hop
{
struct ProfilerStats;
class Server;
class Profiler
{
public:
   enum class SourceType
   {
      NONE,
      FILE,
      PROCESS,
   };

   enum class ViewType
   {
      PROFILER,
      STATS
   };

   Profiler();
   ~Profiler();
   const char* nameAndPID( int* processId = nullptr );
   ProfilerStats stats() const;
   bool setSource( SourceType type, int processId, const char* str );
   SourceType sourceType() const;
   void update( float deltaTimeMs, float globalTimeMs );
   void draw( float drawPosX, float drawPosY, float windowWidth, float windowHeight );
   void fetchClientData();
   void addStringData( const std::vector< char >& stringData );
   void addTraces( const TraceData& traces, uint32_t threadIndex );
   void addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex);
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex);
   void addCoreEvents( const std::vector<CoreEvent>& coreEvents, uint32_t threadIndex );
   void addThreadName( StrPtr_t name, uint32_t threadIndex );
   void handleHotkey();
   void handleMouse();
   void setRecording( bool recording );
   void clear();

   bool saveToFile( const char* path );

private:
   bool openFile( const char* path );
   bool setProcess( int processId, const char* process );

   std::string _name;
   int _pid;
   StringDb _strDb;
   Timeline _timeline;

   // Canvas content
   TimelineTracks _tracks;
   TimelineStats _stats;

   bool _recording{ false };
   SourceType _srcType;
   ViewType _viewType;

   Server _server;
   Server::PendingData _serverPendingData;
};

struct ProfilerStats
{
   size_t strDbSize;
   size_t traceCount;
   size_t clientSharedMemSize;
   int lodLevel;
};

} // namespace hop

#endif  // PROFILER_H_
