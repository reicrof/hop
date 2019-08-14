#ifndef HOP_SERVER_H_
#define HOP_SERVER_H_

#include <Hop.h>
#include "Mutex.h"
#include "StringDb.h"
#include "TraceData.h"

#include <thread>
#include <unordered_map>
#include <vector>

namespace hop
{
class Server
{
  public:
   bool start( int processId, const char* name );
   void setRecording( bool recording );
   void stop();
   void clear();
   const char* processName() const;
   SharedMemory::ConnectionState connectionState() const;
   size_t sharedMemorySize() const;

   struct PendingData
   {
       std::vector<char> stringData;
       std::unordered_map< uint32_t, TraceData > tracesPerThread;
       std::unordered_map< uint32_t, LockWaitData > lockWaitsPerThread;
       std::unordered_map< uint32_t, std::vector<UnlockEvent> > unlockEventsPerThread;
       std::unordered_map< uint32_t, std::vector<CoreEvent> > coreEventsPerThread;

       std::vector< std::pair< uint32_t, StrPtr_t > > threadNames;

       void clear();
       void swap(PendingData& rhs);
   };

   void getPendingData(PendingData& data);

  private:
   // Returns the number of bytes processed
   size_t handleNewMessage( uint8_t* data, size_t maxSize, TimeStamp minTimestamp );
   bool addUniqueThreadName( uint32_t threadIndex, StrPtr_t name );

   void clearPendingMessages();

   std::thread _thread;
   SharedMemory _sharedMem;
   StringDb _stringDb;

   mutable hop::Mutex _stateMutex;
   struct ServerState
   {
      SharedMemory::ConnectionState connectionState;
      std::string processName;
      int32_t pid;
      bool running;
      bool recording;
      bool clearingRequested;
   } _state;

   hop::Mutex _sharedPendingDataMutex;
   PendingData _sharedPendingData;
   std::vector< StrPtr_t > _threadNamesReceived;
};

}  // namespace hop

#endif  // HOP_SERVER_H_