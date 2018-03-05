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

}