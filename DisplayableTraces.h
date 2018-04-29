#ifndef DISPLAYABLE_TRACE_H_
#define DISPLAYABLE_TRACE_H_

#include "Hop.h"
#include "Lod.h"
#include <vector>
#include <deque>
#include <utility>
#include <limits>

namespace hop
{
static constexpr size_t INVALID_IDX = std::numeric_limits<size_t>::max();

struct DisplayableTraces
{
   DisplayableTraces() = default;
   DisplayableTraces(DisplayableTraces&& ) = default;
   DisplayableTraces(const DisplayableTraces& ) = delete;
   DisplayableTraces& operator=(const DisplayableTraces& ) = delete;

   // Explicit copy to avoid accidental one
   DisplayableTraces copy() const;

   void append( const DisplayableTraces& newTraces );
   void clear();

   std::deque< TimeStamp > ends; // in ns
   std::deque< TimeDuration > deltas; // in ns

   //Indexes of the name in the string database
   std::deque< TStrPtr_t > fileNameIds;
   std::deque< TStrPtr_t > fctNameIds;

   std::deque< TLineNb_t > lineNbs;
   std::deque< TGroup_t > groups;
   std::deque< TDepth_t > depths;
   std::deque< uint32_t > flags;

   LodsArray lods;
   TDepth_t maxDepth{ 0 };
};

struct DisplayableLockWaits
{
   DisplayableLockWaits() = default;
   DisplayableLockWaits(DisplayableLockWaits&& ) = default;
   DisplayableLockWaits(const DisplayableLockWaits& ) = delete;
   DisplayableLockWaits& operator=(const DisplayableLockWaits& ) = delete;

   void append( const DisplayableLockWaits& newLockWaits );
   void clear();

   std::deque< TimeStamp > ends; // in ns
   std::deque< TimeDuration > deltas; // in ns
   std::deque< TDepth_t > depths;
   std::deque< void* > mutexAddrs;

   LodsArray lods;
};

template <typename Ts>
std::pair<size_t, size_t>
visibleIndexSpan( const Ts& traces, TimeStamp absoluteStart, TimeStamp absoluteEnd );

std::pair<size_t, size_t>
visibleIndexSpan( const LodsArray& lodsArr, TimeStamp absoluteStart, TimeStamp absoluteEnd, int lodLvl );

}

#endif // DISPLAYABLE_TRACE_H_