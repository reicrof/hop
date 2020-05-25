#ifndef TRACE_DATA_H_
#define TRACE_DATA_H_

#include "Hop.h"
#include "Deque.h"
#include <vector>
#include <utility>
#include <limits>

namespace hop
{
static constexpr size_t INVALID_IDX = std::numeric_limits<size_t>::max();

struct Entries
{
   hop::Deque< TimeStamp > starts;
   hop::Deque< TimeStamp > ends;
   hop::Deque< Depth_t > depths;

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
   hop::Deque< StrPtr_t > fileNameIds;
   hop::Deque< StrPtr_t > fctNameIds;

   hop::Deque< LineNb_t > lineNbs;
   hop::Deque< ZoneId_t > zones;
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
   hop::Deque< void* > mutexAddrs;
   hop::Deque< TimeStamp > lockReleases;
};

struct CoreEventData
{
   CoreEventData() = default;
   CoreEventData(CoreEventData&& ) = default;
   CoreEventData(const CoreEventData& ) = delete;
   CoreEventData& operator=(const CoreEventData& ) = delete;

   void append( const CoreEventData& newCoreEvents );
   void clear();

   Entries entries;
   hop::Deque<Core_t> cores;
};

// Data serialization
size_t serializedSize( const TraceData& td );
size_t serializedSize( const LockWaitData& lw );
size_t serializedSize( const CoreEventData& ced );
size_t serialize( const TraceData& td, char* dst );
size_t serialize( const LockWaitData& lw, char* dst );
size_t serialize( const CoreEventData& ced, char* dst );
size_t deserialize( const char* src, TraceData& td );
size_t deserialize( const char* src, LockWaitData& lw );
size_t deserialize( const char* src, CoreEventData& ced );

}

#endif // TRACE_DATA_H_