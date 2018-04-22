#include "ThreadInfo.h"
#include "Lod.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace hop
{
ThreadInfo::ThreadInfo() { _traces.reserve( 2048 ); }

void ThreadInfo::addTraces( const DisplayableTraces& newTraces )
{
   _traces.append( newTraces );

   assert_is_sorted(_traces.ends.begin(), _traces.ends.end() );
}

void ThreadInfo::addLockWaits( const std::vector<LockWait>& lockWaits )
{
   _lockWaits.insert( _lockWaits.end(), lockWaits.begin(), lockWaits.end() );
}

void ThreadInfo::addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents)
{
    _unlockEvents.insert(_unlockEvents.end(), unlockEvents.begin(), unlockEvents.end());
}

TDepth_t ThreadInfo::maxDepth() const noexcept
{
   return _traces.maxDepth;
}

size_t serializedSize( const ThreadInfo& ti )
{
   const size_t tracesCount = ti._traces.ends.size();
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
   // const size_t serialSize = serializedSize( ti );
   // (void)serialSize; // Removed unused warning
   // const size_t tracesCount = ti._traces.ends.size();

   // size_t i = 0;
   // // Traces count
   // memcpy( &data[i], &tracesCount, sizeof( size_t ) );
   // i += sizeof( size_t );

   // // Max depth
   // memcpy( &data[i], &ti._traces.maxDepth, sizeof( hop::TDepth_t ) );
   // i += sizeof( hop::TDepth_t );

   // //ends
   // memcpy( &data[i], ti._traces.ends.data(), sizeof( hop::TimeStamp ) * tracesCount );
   // i += sizeof( hop::TimeStamp ) * tracesCount;

   // // deltas
   // memcpy( &data[i], ti._traces.deltas.data(), sizeof( hop::TimeDuration ) * tracesCount );
   // i += sizeof( hop::TimeDuration ) * tracesCount;

   // // fileNameIds
   // memcpy( &data[i], ti._traces.fileNameIds.data(), sizeof( hop::TStrPtr_t ) * tracesCount );
   // i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // // fctNameIds
   // memcpy( &data[i], ti._traces.fctNameIds.data(), sizeof( hop::TStrPtr_t ) * tracesCount );
   // i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // // lineNbs
   // memcpy( &data[i], ti._traces.lineNbs.data(), sizeof( hop::TLineNb_t ) * tracesCount );
   // i += sizeof( hop::TLineNb_t ) * tracesCount;

   // // // groups
   // // memcpy( &data[i], ti.traces.groups.data(), sizeof( hop::TGroup_t ) * tracesCount );
   // // i += sizeof( hop::TGroup_t ) * tracesCount;

   // // depths
   // memcpy( &data[i], ti._traces.depths.data(), sizeof( hop::TDepth_t ) * tracesCount );
   // i += sizeof( hop::TDepth_t ) * tracesCount;

   // assert( i == serialSize );

   // return i;
return 0;
}

size_t deserialize( const char* data, ThreadInfo& ti )
{
   // size_t i = 0;
   // const size_t tracesCount = *(size_t*)&data[i];
   // i += sizeof( size_t );
   // ti._traces.maxDepth = *(hop::TDepth_t*)&data[i];
   // i += sizeof( hop::TDepth_t );

   // // Resize the arrays
   // ti._traces.ends.resize( tracesCount );
   // ti._traces.deltas.resize( tracesCount );
   // ti._traces.fileNameIds.resize( tracesCount );
   // ti._traces.fctNameIds.resize( tracesCount );
   // ti._traces.lineNbs.resize( tracesCount );
   // ti._traces.depths.resize( tracesCount );

   // // ends
   // memcpy( ti._traces.ends.data(), &data[i], sizeof( hop::TimeStamp ) * tracesCount );
   // i += sizeof( hop::TimeStamp ) * tracesCount;

   // // deltas
   // memcpy( ti._traces.deltas.data(), &data[i], sizeof( hop::TimeDuration ) * tracesCount );
   // i += sizeof( hop::TimeDuration ) * tracesCount;

   // // fileNameIds
   // memcpy( ti._traces.fileNameIds.data(), &data[i], sizeof( hop::TStrPtr_t ) * tracesCount );
   // i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // // fctNameIds
   // memcpy( ti._traces.fctNameIds.data(), &data[i], sizeof( hop::TStrPtr_t ) * tracesCount );
   // i += sizeof( hop::TStrPtr_t ) * tracesCount;

   // // lineNbs
   // memcpy( ti._traces.lineNbs.data(), &data[i], sizeof( hop::TLineNb_t ) * tracesCount );
   // i += sizeof( hop::TLineNb_t ) * tracesCount;

   // // depths
   // memcpy( ti._traces.depths.data(), &data[i], sizeof( hop::TDepth_t ) * tracesCount );
   // i += sizeof( hop::TDepth_t ) * tracesCount;

   // return i;
return 0;
}

}