#ifndef TRACE_DATA_H_
#define TRACE_DATA_H_

#include "Hop.h"
#include "Deque.h"
#include <vector>
#include <utility>

namespace hop
{
static size_t INVALID_IDX = 0xFFFFFFFFFFFFFFFF;

struct Entries
{
   hop::Deque< hop_timestamp_t > starts;
   hop::Deque< hop_timestamp_t > ends;
   hop::Deque< hop_depth_t > depths;

   void clear();
   void append( const Entries& newEntries );
   Entries copy() const;

   hop_depth_t maxDepth{ 0 };
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
   hop::Deque< hop_str_ptr_t > fileNameIds;
   hop::Deque< hop_str_ptr_t > fctNameIds;

   hop::Deque< hop_linenb_t > lineNbs;
   hop::Deque< hop_zone_t > zones;
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
   hop::Deque< hop_timestamp_t > lockReleases;
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
   hop::Deque<hop_core_t> cores;
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