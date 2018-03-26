#include "ThreadInfo.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace hop
{
ThreadInfo::ThreadInfo() { traces.reserve( 2048 ); }

void ThreadInfo::addTraces( const DisplayableTraces& newTraces )
{
   traces.append( newTraces );

   assert_is_sorted( traces.ends.begin(), traces.ends.end() );
}

void ThreadInfo::addLockWaits( const std::vector<LockWait>& lockWaits )
{
   _lockWaits.insert( _lockWaits.end(), lockWaits.begin(), lockWaits.end() );
}

void ThreadInfo::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

size_t serializedSize( const ThreadInfo& ti )
{
   const size_t tracesCount = ti.traces.ends.size();
   const size_t serializedSize =
       sizeof( size_t ) +                            // Traces count
       sizeof( hop::TDepth_t ) +                     // Max depth
       sizeof( hop::TimeStamp ) * tracesCount +      // ends
       sizeof( hop::TimeDuration ) * tracesCount +   // deltas
       sizeof( hop::TStrPtr_t ) * tracesCount * 2 +  // fileNameId and fctNameIds
       sizeof( hop::TLineNb_t ) * tracesCount +      // lineNbs
       // sizeof( hop::TGroup_t ) * tracesCount +       // groups
       sizeof( hop::TDepth_t ) * tracesCount;  // depths

   return serializedSize;
}

size_t serialize( const ThreadInfo& ti, char* data )
{
   const size_t serialSize = serializedSize( ti );
   (void)serialSize; // Removed unused warning
   const size_t tracesCount = ti.traces.ends.size();

   size_t i = 0;
   // Traces count
   memcpy( &data[i], &tracesCount, sizeof( size_t ) );
   i += sizeof( size_t );

   // Max depth
   memcpy( &data[i], &ti.traces.maxDepth, sizeof( hop::TDepth_t ) );
   i += sizeof( hop::TDepth_t );

   //ends
   memcpy( &data[i], ti.traces.ends.data(), sizeof( hop::TimeStamp ) * tracesCount );
   i += sizeof( hop::TimeStamp ) * tracesCount;

   // deltas
   memcpy( &data[i], ti.traces.deltas.data(), sizeof( hop::TimeDuration ) * tracesCount );
   i += sizeof( hop::TimeDuration ) * tracesCount;

   // fileNameIds
   memcpy( &data[i], ti.traces.fileNameIds.data(), sizeof( hop::TStrPtr_t ) * tracesCount );
   i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // fctNameIds
   memcpy( &data[i], ti.traces.fctNameIds.data(), sizeof( hop::TStrPtr_t ) * tracesCount );
   i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // lineNbs
   memcpy( &data[i], ti.traces.lineNbs.data(), sizeof( hop::TLineNb_t ) * tracesCount );
   i += sizeof( hop::TLineNb_t ) * tracesCount;

   // // groups
   // memcpy( &data[i], ti.traces.groups.data(), sizeof( hop::TGroup_t ) * tracesCount );
   // i += sizeof( hop::TGroup_t ) * tracesCount;

   // depths
   memcpy( &data[i], ti.traces.depths.data(), sizeof( hop::TDepth_t ) * tracesCount );
   i += sizeof( hop::TDepth_t ) * tracesCount;

   assert( i == serialSize );

   return i;
}

size_t deserialize( const char* data, ThreadInfo& ti )
{
   size_t i = 0;
   const size_t tracesCount = *(size_t*)&data[i];
   i += sizeof( size_t );
   ti.traces.maxDepth = *(hop::TDepth_t*)&data[i];
   i += sizeof( hop::TDepth_t );

   // Resize the arrays
   ti.traces.ends.resize( tracesCount );
   ti.traces.deltas.resize( tracesCount );
   ti.traces.fileNameIds.resize( tracesCount );
   ti.traces.fctNameIds.resize( tracesCount );
   ti.traces.lineNbs.resize( tracesCount );
   ti.traces.depths.resize( tracesCount );

   // ends
   memcpy( ti.traces.ends.data(), &data[i], sizeof( hop::TimeStamp ) * tracesCount );
   i += sizeof( hop::TimeStamp ) * tracesCount;

   // deltas
   memcpy( ti.traces.deltas.data(), &data[i], sizeof( hop::TimeDuration ) * tracesCount );
   i += sizeof( hop::TimeDuration ) * tracesCount;

   // fileNameIds
   memcpy( ti.traces.fileNameIds.data(), &data[i], sizeof( hop::TStrPtr_t ) * tracesCount );
   i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // fctNameIds
   memcpy( ti.traces.fctNameIds.data(), &data[i], sizeof( hop::TStrPtr_t ) * tracesCount );
   i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // lineNbs
   memcpy( ti.traces.lineNbs.data(), &data[i], sizeof( hop::TLineNb_t ) * tracesCount );
   i += sizeof( hop::TLineNb_t ) * tracesCount;

   // depths
   memcpy( ti.traces.depths.data(), &data[i], sizeof( hop::TDepth_t ) * tracesCount );
   i += sizeof( hop::TDepth_t ) * tracesCount;

   return i;
}

}