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

   enum Flags
   {
      END_TRACE = 0,
      START_TRACE = 1,
   };

   // Explicit copy to avoid accidental one
   DisplayableTraces copy() const;

   void append( const DisplayableTraces& newTraces );
   void reserve( size_t size );
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

std::pair<size_t, size_t> visibleTracesIndexSpan(
    const DisplayableTraces& traces,
    TimeStamp absoluteStart,
    TimeStamp absoluteEnd );
}

#endif // DISPLAYABLE_TRACE_H_