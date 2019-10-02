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

struct Entries
{
   std::deque< TimeStamp > ends; // in ns
   std::deque< TimeDuration > deltas; // in ns
   std::deque< Depth_t > depths;

   void clear();
   void append( const Entries& newEntries );
   Entries copy() const;

   Depth_t maxDepth{ 0 };
};

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

   Entries entries;

   //Indexes of the name in the string database
   std::deque< StrPtr_t > fileNameIds;
   std::deque< StrPtr_t > fctNameIds;

   std::deque< LineNb_t > lineNbs;
   std::deque< ZoneId_t > zones;

   LodsArray lods;
};

struct LockWaitData
{
   LockWaitData() = default;
   LockWaitData(LockWaitData&& ) = default;
   LockWaitData(const LockWaitData& ) = delete;
   LockWaitData& operator=(const LockWaitData& ) = delete;

   void append( const LockWaitData& newLockWaits );
   void clear();

   Entries entries;
   std::deque< void* > mutexAddrs;
   std::deque< TimeStamp > lockReleases;

   LodsArray lods;
};

struct CoreEventData
{
   std::deque<CoreEvent> data;
};

struct StatEventData
{
   std::deque<TimeStamp> times;
   std::deque<StrPtr_t> names;
   std::deque<StatEventValue> values;
   std::deque<StatEventValueType> valueTypes;
};

std::pair<size_t, size_t>
visibleIndexSpan( const LodsArray& lodsArr, int lodLvl, TimeStamp absoluteStart, TimeStamp absoluteEnd, int baseDepth );

// Data serialization
size_t serializedSize( const TraceData& td );
size_t serializedSize( const LockWaitData& lw );
size_t serialize( const TraceData& td, char* dst );
size_t serialize( const LockWaitData& lw, char* dst );
size_t deserialize( const char* src, TraceData& td );
size_t deserialize( const char* src, LockWaitData& lw );

}

#endif // TRACE_DATA_H_