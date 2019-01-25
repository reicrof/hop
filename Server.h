#ifndef HOP_SERVER_H_
#define HOP_SERVER_H_

#include <Hop.h>
#include "Mutex.h"
#include "StringDb.h"
#include "TraceData.h"

#include <vector>
#include <thread>
#include <atomic>

namespace hop
{
class Server
{
  public:
   bool start( const char* name );
   void setRecording( bool recording );
   void stop();
   void clear();
   SharedMemory::ConnectionState connectionState() const;

   struct PendingData
   {
       std::vector< TraceData > traces;
       std::vector<std::vector<char> > stringData;
       std::vector<uint32_t> tracesThreadIndex;

       std::vector< LockWaitData > lockWaits;
       std::vector<uint32_t> lockWaitThreadIndex;

       std::vector<std::vector<UnlockEvent> > unlockEvents;
       std::vector<uint32_t> unlockEventsThreadIndex;

       std::vector<std::vector<CoreEvent> > coreEvents;
       std::vector<uint32_t> coreEventsThreadIndex;

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
   std::atomic< bool > _running{false};
   std::atomic< bool > _recording{false};
   SharedMemory _sharedMem;
   std::atomic< SharedMemory::ConnectionState > _connectionState;

   std::atomic< bool > _clearingRequested{false};
   StringDb _stringDb;

   hop::Mutex _sharedPendingDataMutex;
   PendingData _sharedPendingData;
   std::vector< StrPtr_t > _threadNamesReceived;
};

}  // namespace hop

#endif  // HOP_SERVER_H_