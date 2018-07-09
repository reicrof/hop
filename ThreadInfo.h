#ifndef THREAD_INFO_H_
#define THREAD_INFO_H_

#include "Hop.h"
#include "DisplayableTraces.h"

#include <vector>

namespace hop
{

struct ThreadInfo
{
   void addTraces( const DisplayableTraces& traces );
   void addLockWaits( const DisplayableLockWaits& lockWaits );
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents);
   TDepth_t maxDepth() const noexcept;
   float maxDisplayedDepth() const noexcept;
   void setTrackHeight( float height );
   DisplayableTraces _traces;
   DisplayableLockWaits _lockWaits;
   std::vector<UnlockEvent> _unlockEvents;

   // This is the position at which the traces for the current thread ifno
   // are being draw
   float _localTracesVerticalStartPos;
   float _absoluteTracesVerticalStartPos;
   float _trackHeight{9999.0f};

   friend size_t serializedSize( const ThreadInfo& ti );
   friend size_t serialize( const ThreadInfo& ti, char* );
   friend size_t deserialize( const char* data, ThreadInfo& ti );
};
}

#endif  // THREAD_INFO_H_
