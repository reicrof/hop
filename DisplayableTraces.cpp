#include "DisplayableTraces.h"

#include <algorithm>
#include <cassert>

namespace vdbg
{

void DisplayableTraces::append( const DisplayableTraces& newTraces )
{
   deltas.insert( deltas.end(), newTraces.deltas.begin(), newTraces.deltas.end() );
   ends.insert( ends.end(), newTraces.ends.begin(), newTraces.ends.end() );
   flags.insert( flags.end(), newTraces.flags.begin(), newTraces.flags.end() );
   fileNameIds.insert(
       fileNameIds.end(), newTraces.fileNameIds.begin(), newTraces.fileNameIds.end() );
   classNameIds.insert(
       classNameIds.end(), newTraces.classNameIds.begin(), newTraces.classNameIds.end() );
   fctNameIds.insert( fctNameIds.end(), newTraces.fctNameIds.begin(), newTraces.fctNameIds.end() );
   lineNbs.insert( lineNbs.end(), newTraces.lineNbs.begin(), newTraces.lineNbs.end() );
   depths.insert( depths.end(), newTraces.depths.begin(), newTraces.depths.end() );

   auto lods = computeLods( newTraces );
   for ( size_t i = 0; i < lods.size(); ++i )
   {
      auto it = std::lower_bound( _lods[i].begin(), _lods[i].end(), lods[i].front() );
      auto sortFromIdx = std::distance( _lods[i].begin(), it );
      _lods[i].insert( _lods[i].end(), lods[i].begin(), lods[i].end() );
      std::sort( _lods[i].begin() + sortFromIdx, _lods[i].end() );

      assert( std::is_sorted( _lods[i].begin(), _lods[i].end() ) );
   }
}

void DisplayableTraces::reserve( size_t size )
{
   ends.reserve( size );
   deltas.reserve( size );
   flags.reserve( size );
   fileNameIds.reserve( size );
   classNameIds.reserve( size );
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
   classNameIds.clear();
   fctNameIds.clear();
   lineNbs.clear();
   depths.clear();
}

}