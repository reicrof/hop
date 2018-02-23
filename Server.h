#ifndef HOP_SERVER_H_
#define HOP_SERVER_H_

#include <Hop.h>
#include <Profiler.h>
#include "StringDb.h"

#include <vector>
#include <thread>
#include <mutex>

namespace hop
{
class Server
{
  public:
   bool start( const char* name );
   void setRecording( bool recording );
   void stop();

   void getPendingProfilingTraces(
       std::vector< DisplayableTraces >& tracesFrame,
       std::vector<std::vector<char> >& stringData,
       std::vector<uint32_t>& threadIds );
   void getPendingLockWaits(
       std::vector<std::vector<LockWait> >& lockWaits,
       std::vector<uint32_t>& threadIds );
   void getPendingUnlockEvents(
       std::vector<std::vector<UnlockEvent> >& unlockEvents,
       std::vector<uint32_t>& threadIds );

  private:
   // Returns the number of bytes processed
   size_t handleNewMessage( uint8_t* data, size_t maxSize );

   std::thread _thread;
   bool _running{false};
   SharedMemory _sharedMem;
   StringDb stringDb;

   std::mutex pendingTracesMutex;
   std::vector< DisplayableTraces > pendingTraces;
   std::vector<std::vector<char> > pendingStringData;
   std::vector<uint32_t> pendingThreadIds;

   std::mutex pendingLockWaitsMutex;
   std::vector<uint32_t> pendingLockWaitThreadIds;
   std::vector<std::vector<LockWait> > pendingLockWaits;

   std::mutex pendingUnlockEventsMutex;
   std::vector<uint32_t> pendingUnlockEventsThreadIds;
   std::vector<std::vector<UnlockEvent> > pendingUnlockEvents;
};

}  // namespace hop

#endif  // HOP_SERVER_H_