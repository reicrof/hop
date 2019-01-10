#include "TraceData.h"

#include <algorithm>
#include <cassert>

namespace hop
{

void Entries::clear()
{
   ends.clear();
   deltas.clear();
   depths.clear();
   maxDepth = 0;
}

void Entries::append( const Entries& newEntries )
{
   deltas.insert( deltas.end(), newEntries.deltas.begin(), newEntries.deltas.end() );
   ends.insert( ends.end(), newEntries.ends.begin(), newEntries.ends.end() );
   depths.insert( depths.end(), newEntries.depths.begin(), newEntries.depths.end() );

   maxDepth = std::max( maxDepth, newEntries.maxDepth );
}

Entries Entries::copy() const
{
   Entries copy;
   copy.ends = this->ends;
   copy.deltas = this->deltas;
   copy.maxDepth = this->maxDepth;
   copy.depths = this->depths;
   return copy;
}

void TraceData::append( const TraceData& newTraces )
{
   const size_t prevSize = entries.deltas.size();

   entries.append( newTraces.entries );
   fileNameIds.insert(
       fileNameIds.end(), newTraces.fileNameIds.begin(), newTraces.fileNameIds.end() );
   fctNameIds.insert( fctNameIds.end(), newTraces.fctNameIds.begin(), newTraces.fctNameIds.end() );
   lineNbs.insert( lineNbs.end(), newTraces.lineNbs.begin(), newTraces.lineNbs.end() );
   zones.insert( zones.end(), newTraces.zones.begin(), newTraces.zones.end() );

   appendLods( lods, computeLods( newTraces.entries, prevSize ) );
}

void TraceData::clear()
{
   entries.clear();
   fileNameIds.clear();
   fctNameIds.clear();
   lineNbs.clear();
   zones.clear();
   for( auto& dq : lods )
      dq.clear();
}

TraceData TraceData::copy() const
{
   TraceData copy;
   copy.entries = this->entries.copy();

   copy.fileNameIds = this->fileNameIds;
   copy.fctNameIds = this->fctNameIds;
   copy.lineNbs = this->lineNbs;
   copy.zones = this->zones;
   copy.lods = this->lods;
   return copy;
}

void LockWaitData::append( const LockWaitData& newLockWaits )
{
   const size_t prevSize = entries.ends.size();

   entries.append( newLockWaits.entries );
   mutexAddrs.insert( mutexAddrs.end(), newLockWaits.mutexAddrs.begin(), newLockWaits.mutexAddrs.end() );

   const size_t lockReleaseSize = newLockWaits.lockReleases.size();
   if( lockReleaseSize == 0 )
   {
      // Append 0 for lock releases. They will be filled when the unlock event are received
      lockReleases.resize(
          lockReleases.size() + std::distance( newLockWaits.entries.ends.begin(), newLockWaits.entries.ends.end() ),
          0 );
   }
   else
   {
      // We are probably reading from a file since the lockrealse are already processed.
      assert( lockReleaseSize == newLockWaits.entries.ends.size() );
      lockReleases.insert( lockReleases.end(), newLockWaits.lockReleases.begin(), newLockWaits.lockReleases.end() );
   }

   appendLods( lods, computeLods( newLockWaits.entries, prevSize ) );
}

void LockWaitData::clear()
{
   entries.clear();
   mutexAddrs.clear();
   lockReleases.clear();
   for( auto& dq : lods )
      dq.clear();
}

std::pair<size_t, size_t> visibleIndexSpan(
    const LodsArray& lodsArr,
    int lodLvl,
    TimeStamp absoluteStart,
    TimeStamp absoluteEnd,
    int baseDepth )
{
   auto span = std::make_pair( hop::INVALID_IDX, hop::INVALID_IDX );

   const auto& lods = lodsArr[lodLvl];
   const LodInfo firstInfo = {absoluteStart, 0, 0, 0, false};
   const LodInfo lastInfo = {absoluteEnd, 0, 0, 0, false};
   auto it1 = std::lower_bound( lods.begin(), lods.end(), firstInfo );
   auto it2 = std::upper_bound( lods.begin(), lods.end(), lastInfo );

   // The last trace of the current thread does not reach the current time
   if ( it1 == lods.end() ) return span;

   // Find the the first trace on right that have a depth of "baseDepth". This can be either 0
   // for traces or 1 for lockwaits. This prevents traces that have a smaller depth than the
   // one foune previously to vanish.
   while ( it2 != lods.end() && it2->depth != baseDepth )
   {
      ++it2;
   }
   if ( it2 != lods.end() )
   {
      ++it2;
   }  // We need to go one past the depth 0

   span.first = std::distance( lods.begin(), it1 );
   span.second = std::distance( lods.begin(), it2 );
   return span;
}

} // namespace hop