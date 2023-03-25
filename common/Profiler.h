#ifndef HOP_PROFILER_H_
#define HOP_PROFILER_H_

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
      SRC_TYPE_NETWORK,
   };

   Profiler( SourceType type, int processId, const char* str );
#if HOP_USE_REMOTE_PROFILER
   Profiler( NetworkConnection& nc );
   const NetworkConnection* networkConnection() const;
#endif
   ~Profiler();
   bool operator==( const Profiler& rhs ) const;

   const char* nameAndPID( int* processId = nullptr, bool shortName = false ) const;
   float cpuFreqGHz() const;
   ProfilerStats stats() const;
   SourceType sourceType() const;
   bool recording() const;
   ConnectionState connectionState() const;
   const std::vector<TimelineTrack>& timelineTracks() const;
   const StringDb& stringDb() const;
   TimeStamp earliestTimestamp() const;
   TimeStamp latestTimestamp() const;

   void setRecording( bool recording );
   bool fetchClientData();
   bool addStringData( const std::vector< char >& stringData );
   bool addTraces( const TraceData& traces, uint32_t threadIndex );
   bool addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex);
   bool addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex);
   bool addCoreEvents( const CoreEventData& coreEvents, uint32_t threadIndex );
   void addThreadName( StrPtr_t name, uint32_t threadIndex );
   void clear();

   bool saveToFile( const char* path );
   bool openFile( const char* path );

private:
   std::string _name;
   std::vector<TimelineTrack> _tracks;
   StringDb _strDb;
   bool _recording;
   SourceType _srcType;
   float _loadedFileCpuFreqGHz;

   Server _server;
   Server::PendingData _serverPendingData;

   TimeStamp _earliestTimeStamp;
   TimeStamp _latestTimeStamp;
};

}  // namespace hop

#endif // HOP_PROFILER_H_
