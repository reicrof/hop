#include "DisplayableTraces.h"

#include <algorithm>
#include <cassert>

namespace hop
{

void DisplayableTraces::append( const DisplayableTraces& newTraces )
{
   const size_t prevSize = deltas.size();

   deltas.insert( deltas.end(), newTraces.deltas.begin(), newTraces.deltas.end() );
   ends.insert( ends.end(), newTraces.ends.begin(), newTraces.ends.end() );
   flags.insert( flags.end(), newTraces.flags.begin(), newTraces.flags.end() );
   fileNameIds.insert(
       fileNameIds.end(), newTraces.fileNameIds.begin(), newTraces.fileNameIds.end() );
   fctNameIds.insert( fctNameIds.end(), newTraces.fctNameIds.begin(), newTraces.fctNameIds.end() );
   lineNbs.insert( lineNbs.end(), newTraces.lineNbs.begin(), newTraces.lineNbs.end() );
   depths.insert( depths.end(), newTraces.depths.begin(), newTraces.depths.end() );
   maxDepth = std::max(maxDepth, newTraces.maxDepth);

   appendLods( lods, computeLods( newTraces, prevSize ) );
}

void DisplayableTraces::reserve( size_t size )
{
   ends.reserve( size );
   deltas.reserve( size );
   flags.reserve( size );
   fileNameIds.reserve( size );
   fctNameIds.reserve( size );
   lineNbs.reserve( size );
   depths.reserve( size );
}

void DisplayableTraces::clear()
{
   ends.clear();
   deltas.clear();
   flags.clear();
   fileNameIds.clear();
   fctNameIds.clear();
   lineNbs.clear();
   depths.clear();
   maxDepth = 0;
}

std::pair<size_t, size_t> visibleTracesIndexSpan(
    const DisplayableTraces& traces,
    TimeStamp absoluteStart,
    TimeStamp absoluteEnd )
{
   auto span = std::make_pair( hop::INVALID_IDX, hop::INVALID_IDX );
   const auto it1 = std::lower_bound( traces.ends.begin(), traces.ends.end(), absoluteStart );
   const auto it2 = std::upper_bound( traces.ends.begin(), traces.ends.end(), absoluteEnd );

   // The last trace does not reach the specified time range
   if ( it1 == traces.ends.end() ) return span;

   span.first = std::distance( traces.ends.begin(), it1 );
   span.second = std::distance( traces.ends.begin(), it2 );

   // Find the the first trace on both size with depths of zero to prevent returning
   // part of the trace stack
   while ( span.first > 0 && traces.depths[span.first] != 0 )
   {
      --span.first;
   }
   while ( span.second < traces.depths.size() && traces.depths[span.second] != 0 )
   {
      ++span.second;
   }
    // We need to go one past the depth 0
   if ( span.second < traces.depths.size() )
   {
      ++span.second;
   }

   return span;
}
}