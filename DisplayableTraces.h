#ifndef DISPLAYABLE_TRACE_H_
#define DISPLAYABLE_TRACE_H_

#include "Hop.h"
#include "Lod.h"
#include <vector>
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

   void append( const DisplayableTraces& newTraces );
   void reserve( size_t size );
   void clear();

   std::vector< TimeStamp > ends; // in ns
   std::vector< TimeDuration > deltas; // in ns

   //Indexes of the name in the string database
   std::vector< TStrPtr_t > fileNameIds;
   std::vector< TStrPtr_t > fctNameIds;

   std::vector< TLineNb_t > lineNbs;
   std::vector< TGroup_t > groups;
   std::vector< TDepth_t > depths;
   std::vector< uint32_t > flags;

   LodsArray lods;
   TDepth_t maxDepth{ 0 };
};

std::pair<size_t, size_t> visibleTracesIndexSpan(
    const DisplayableTraces& traces,
    TimeStamp absoluteStart,
    TimeStamp absoluteEnd );
}

#endif // DISPLAYABLE_TRACE_H_