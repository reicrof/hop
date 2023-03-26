#ifndef HOP_SERVER_H_
#define HOP_SERVER_H_

#define HOP_VIEWER
#include <Hop.h>

#include "common/Mutex.h"
#include "common/StringDb.h"
#include "common/TraceData.h"

#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hop
{
class Transport;
class Server
{
  public:
   bool start( int processId, const char* name );
#if HOP_USE_REMOTE_PROFILER
   bool start( std::unique_ptr<NetworkConnection> nc );
   const NetworkConnection* networkConnection() const;
#endif
   void setRecording( bool recording );
   void stop();
   void clear();
   const char* processInfo( int* processId ) const;
   const char* shortProcessInfo( int* processId ) const;
   ConnectionState connectionState() const;
   size_t sharedMemorySize() const;
   float cpuFreqGHz() const;

   struct PendingData
   {
       std::vector<char> stringData;
       std::unordered_map< uint32_t, TraceData > tracesPerThread;
       std::unordered_map< uint32_t, LockWaitData > lockWaitsPerThread;
       std::unordered_map< uint32_t, std::vector<UnlockEvent> > unlockEventsPerThread;
       std::unordered_map< uint32_t, CoreEventData > coreEventsPerThread;

       std::vector< std::pair< uint32_t, StrPtr_t > > threadNames;

       void clear();
       void swap(PendingData& rhs);
   };

   void getPendingData(PendingData& data);

   Transport *_transport = nullptr;

   // Return wether or not we should retry to connect and fill the connection state
   bool tryConnect( int32_t pid, ConnectionState& newState );

   // Returns the bytes read
   ssize_t handleNewMessage( const uint8_t* data, size_t maxSize, uint32_t &seed );
   bool addUniqueThreadName( uint32_t threadIndex, StrPtr_t name );

   void clearPendingMessages();

   std::thread _thread;
   StringDb _stringDb;

   mutable float _cpuFreqGHz{0};
   mutable hop::Mutex _stateMutex;
   struct ServerState
   {
      ConnectionState connectionState;
      std::string processName;
      uint16_t shortNameIndex{0}; // Short name starting index
      int pid{-1};
      uint32_t seed{ 0 };
      bool running{false};
      bool recording{false};
   } _state;

   hop::Mutex _sharedPendingDataMutex;
   PendingData _sharedPendingData;
   std::vector< StrPtr_t > _threadNamesReceived;
};

}  // namespace hop

#endif  // HOP_SERVER_H_