#ifndef TRACE_DATA_H_
#define TRACE_DATA_H_

#include "Hop.h"
#include "Lod.h"
#include <vector>
#include <deque>
#include <utility>
#include <limits>

namespace hop
{
static constexpr size_t INVALID_IDX = std::numeric_limits<size_t>::max();

struct TraceData
{
   TraceData() = default;
   TraceData(TraceData&& ) = default;
   TraceData(const TraceData& ) = delete;
   TraceData& operator=(const TraceData& ) = delete;

   // Explicit copy to avoid accidental one
   TraceData copy() const;

   void append( const TraceData& newTraces );
   void clear();

   std::deque< TimeStamp > ends; // in ns
   std::deque< TimeDuration > deltas; // in ns

   //Indexes of the name in the string database
   std::deque< TStrPtr_t > fileNameIds;
   std::deque< TStrPtr_t > fctNameIds;

   std::deque< TLineNb_t > lineNbs;
   std::deque< TZoneId_t > zones;
   std::deque< TDepth_t > depths;

   LodsArray lods;
   TDepth_t maxDepth{ 0 };
};

struct LockWaitData
{
   LockWaitData() = default;
   LockWaitData(LockWaitData&& ) = default;
   LockWaitData(const LockWaitData& ) = delete;
   LockWaitData& operator=(const LockWaitData& ) = delete;

   void append( const LockWaitData& newLockWaits );
   void clear();

   std::deque< TimeStamp > ends; // in ns
   std::deque< TimeDuration > deltas; // in ns
   std::deque< TDepth_t > depths;
   std::deque< void* > mutexAddrs;
   std::deque< TimeStamp > lockReleases;

   LodsArray lods;
};

template <typename Ts>
std::pair<size_t, size_t>
visibleIndexSpan( const Ts& traces, TimeStamp absoluteStart, TimeStamp absoluteEnd );

std::pair<size_t, size_t>
visibleIndexSpan( const LodsArray& lodsArr, int lodLvl, TimeStamp absoluteStart, TimeStamp absoluteEnd, int baseDepth );

}

#endif // TRACE_DATA_H_