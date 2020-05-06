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

struct hop_shared_memory;

namespace hop
{
class Server
{
  public:
   bool start( int processId, const char* name );
   void setRecording( bool recording );
   void stop();
   void clear();
   const char* processInfo( int* processId ) const;
   hop_connection_state connectionState() const;
   size_t sharedMemorySize() const;
   float cpuFreqGHz() const;

   struct PendingData
   {
       std::vector<char> stringData;
       std::unordered_map< uint32_t, TraceData > tracesPerThread;
       std::unordered_map< uint32_t, LockWaitData > lockWaitsPerThread;
       std::unordered_map< uint32_t, std::vector<hop_unlock_event_t> > unlockEventsPerThread;
       std::unordered_map< uint32_t, CoreEventData > coreEventsPerThread;

       std::vector< std::pair< uint32_t, hop_str_ptr_t > > threadNames;

       void clear();
       void swap(PendingData& rhs);
   };

   void getPendingData(PendingData& data);

  private:
   // Return wether or not we should retry to connect and fill the connection state
   bool tryConnect( int32_t pid, hop_connection_state& newState );

   // Returns the number of bytes processed
   size_t handleNewMessage( uint8_t* data, size_t maxSize, hop_timestamp_t minTimestamp );
   bool addUniqueThreadName( uint32_t threadIndex, hop_str_ptr_t name );

   void clearPendingMessages();

   std::thread _thread;
   hop_shared_memory* _sharedMem{nullptr};
   StringDb _stringDb;

   mutable hop::Mutex _stateMutex;
   struct ServerState
   {
      hop_connection_state connectionState;
      std::string processName;
      int pid{-1};
      bool running{false};
      bool recording{false};
      bool clearingRequested{false};
   } _state;

   hop::Mutex _sharedPendingDataMutex;
   PendingData _sharedPendingData;
   std::vector< hop_str_ptr_t > _threadNamesReceived;
};

}  // namespace hop

#endif  // HOP_SERVER_H_