#ifndef HOP_SERVER_H_
#define HOP_SERVER_H_

#include <Hop.h>
#include "StringDb.h"
#include "DisplayableTraces.h"

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace hop
{
class Server
{
  public:
   bool start( const char* name );
   bool setRecording( bool recording );
   void stop();

   struct PendingData
   {
       std::mutex mutex;
       std::vector< DisplayableTraces > traces;
       std::vector<std::vector<char> > stringData;
       std::vector<uint32_t> tracesThreadIndex;

       std::vector<std::vector<LockWait> > lockWaits;
       std::vector<uint32_t> lockWaitThreadIndex;

       std::vector<std::vector<UnlockEvent> > unlockEvents;
       std::vector<uint32_t> unlockEventsThreadIndex;

       void clear();
       void swap(PendingData& rhs);
   };

   void getPendingData(PendingData& data);

  private:
   // Returns the number of bytes processed
   size_t handleNewMessage( uint8_t* data, size_t maxSize );

   std::thread _thread;
   std::atomic< bool > _running{false};
   SharedMemory _sharedMem;
   StringDb _stringDb;

   PendingData _pendingData;
};

}  // namespace hop

#endif  // HOP_SERVER_H_