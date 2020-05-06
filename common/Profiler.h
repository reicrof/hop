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
   };

   Profiler( SourceType type, int processId, const char* str );
   ~Profiler();

   const char* nameAndPID( int* processId = nullptr ) const;
   float cpuFreqGHz() const;
   ProfilerStats stats() const;
   SourceType sourceType() const;
   bool recording() const;
   hop_connection_state connectionState() const;
   const std::vector<TimelineTrack>& timelineTracks() const;
   const StringDb& stringDb() const;
   hop_timestamp_t earliestTimestamp() const;
   hop_timestamp_t latestTimestamp() const;

   void setRecording( bool recording );
   void fetchClientData();
   void addStringData( const std::vector< char >& stringData );
   void addTraces( const TraceData& traces, uint32_t threadIndex );
   void addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex);
   void addUnlockEvents(const std::vector<hop_unlock_event_t>& unlockEvents, uint32_t threadIndex);
   void addCoreEvents( const CoreEventData& coreEvents, uint32_t threadIndex );
   void addThreadName( hop_str_ptr_t name, uint32_t threadIndex );
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

   hop_timestamp_t _earliestTimeStamp;
   hop_timestamp_t _latestTimeStamp;
};

}  // namespace hop

#endif // HOP_PROFILER_H_
