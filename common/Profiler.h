#ifndef PROFILER_H_
#define PROFILER_H_

#include "common/Server.h" // Will include Hop.h with the HOP_VIEWER defined
#include "common/StringDb.h"
#include "common/TimelineTrack.h"

#include <string>

namespace hop
{

struct ProfilerStats
{
   size_t strDbSize;
   size_t traceCount;
   size_t clientSharedMemSize;
};

class Profiler
{
public:
   enum SourceType
   {
      SRC_TYPE_NONE,
      SRC_TYPE_FILE,
      SRC_TYPE_PROCESS,
   };

   Profiler( SourceType type, int processId, const char* str );
   ~Profiler();

   const char* nameAndPID( int* processId = nullptr ) const;
   ProfilerStats stats() const;
   SourceType sourceType() const;
   bool recording() const;
   SharedMemory::ConnectionState connectionState() const;
   const std::vector<TimelineTrack>& timelineTracks() const;
   const StringDb& stringDb() const;
   TimeStamp earliestTimestamp() const;
   TimeStamp latestTimestamp() const;

   void setRecording( bool recording );
   void fetchClientData();
   void addStringData( const std::vector< char >& stringData );
   void addTraces( const TraceData& traces, uint32_t threadIndex );
   void addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex);
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex);
   void addCoreEvents( const std::vector<CoreEvent>& coreEvents, uint32_t threadIndex );
   void addThreadName( StrPtr_t name, uint32_t threadIndex );
   void clear();

   bool saveToFile( const char* path );
   bool openFile( const char* path );

private:
   std::string _name;
   int _pid;
   std::vector<TimelineTrack> _tracks;
   StringDb _strDb;
   bool _recording{ false };
   SourceType _srcType;

   Server _server;
   Server::PendingData _serverPendingData;

   TimeStamp _earliestTimeStamp;
   TimeStamp _latestTimeStamp;
};

}  // namespace hop

#endif // PROFILER_H_