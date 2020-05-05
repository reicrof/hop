#include "TraceData.h"

#include <algorithm>
#include <cassert>
#include <cstring> //memcpy

namespace hop
{

void Entries::clear()
{
   starts.clear();
   ends.clear();
   depths.clear();
   maxDepth = 0;
}

void Entries::append( const Entries& newEntries )
{
   starts.append( newEntries.starts.begin(), newEntries.starts.end() );
   ends.append( newEntries.ends.begin(), newEntries.ends.end() );
   depths.append( newEntries.depths.begin(), newEntries.depths.end() );

   maxDepth = std::max( maxDepth, newEntries.maxDepth );
}

Entries Entries::copy() const
{
   Entries copy;
   copy.ends = this->ends;
   copy.starts = this->starts;
   copy.maxDepth = this->maxDepth;
   copy.depths = this->depths;
   return copy;
}

void TraceData::append( const TraceData& newTraces )
{
   entries.append( newTraces.entries );
   fileNameIds.append( newTraces.fileNameIds.begin(), newTraces.fileNameIds.end() );
   fctNameIds.append( newTraces.fctNameIds.begin(), newTraces.fctNameIds.end() );
   lineNbs.append( newTraces.lineNbs.begin(), newTraces.lineNbs.end() );
   zones.append( newTraces.zones.begin(), newTraces.zones.end() );
}

void TraceData::clear()
{
   entries.clear();
   fileNameIds.clear();
   fctNameIds.clear();
   lineNbs.clear();
   zones.clear();
}

TraceData TraceData::copy() const
{
   TraceData copy;
   copy.entries = this->entries.copy();

   copy.fileNameIds = this->fileNameIds;
   copy.fctNameIds = this->fctNameIds;
   copy.lineNbs = this->lineNbs;
   copy.zones = this->zones;
   return copy;
}

void LockWaitData::append( const LockWaitData& newLockWaits )
{
   entries.append( newLockWaits.entries );
   mutexAddrs.append( newLockWaits.mutexAddrs.begin(), newLockWaits.mutexAddrs.end() );

   const size_t lockReleaseSize = newLockWaits.lockReleases.size();
   if( lockReleaseSize == 0 )
   {
      // Append 0 for lock releases. They will be filled when the unlock event are received
      const uint32_t newLockCount = std::distance( newLockWaits.entries.ends.begin(), newLockWaits.entries.ends.end() );
      lockReleases.append( newLockCount, 0 );
   }
   else
   {
      // We are probably reading from a file since the lockrealse are already processed.
      assert( lockReleaseSize == newLockWaits.entries.ends.size() );
      lockReleases.append( newLockWaits.lockReleases.begin(), newLockWaits.lockReleases.end() );
   }
}

void LockWaitData::clear()
{
   entries.clear();
   mutexAddrs.clear();
   lockReleases.clear();
}


void CoreEventData::append( const CoreEventData& newCoreEvents )
{
   entries.append( newCoreEvents.entries );
   cores.append( newCoreEvents.cores.begin(), newCoreEvents.cores.end() );
}

void CoreEventData::clear()
{
   entries.clear();
   cores.clear();
}

static size_t serializedSize( const hop::Entries& entries )
{
   const size_t entriesCount = entries.ends.size();
   const size_t size = sizeof( hop_depth_t ) +                        // Max depth
                       sizeof( hop_timestamp_t ) * entriesCount +       // starts
                       sizeof( hop_timestamp_t ) * entriesCount +       // ends
                       sizeof( hop_depth_t ) * entriesCount;          // depths
   return size;
}

static size_t serialize( const hop::Entries& entries, char* dst )
{
   size_t i = 0;

   const size_t tracesCount = entries.ends.size();
   (void)tracesCount;

   // Max depth
   memcpy( &dst[i], &entries.maxDepth, sizeof( hop_depth_t ) );
   i += sizeof( hop_depth_t );

   // ends
   {
      std::copy( entries.ends.begin(), entries.ends.end(), (hop_timestamp_t*)&dst[i] );
      i += sizeof( hop_timestamp_t ) * tracesCount;
   }

   // starts
   {
      std::copy( entries.starts.begin(), entries.starts.end(), (hop_timestamp_t*)&dst[i] );
      i += sizeof( hop_timestamp_t ) * tracesCount;
   }

   // depths
   {
      std::copy( entries.depths.begin(), entries.depths.end(), (hop_depth_t*)&dst[i] );
      i += sizeof( hop_depth_t ) * tracesCount;
   }

   return i;
}

static size_t deserialize( const char* src, size_t count, hop::Entries& entries )
{
   size_t i = 0;

   entries.maxDepth = *(hop_depth_t*)&src[i];
   i += sizeof( hop_depth_t );

   {  // ends
      std::copy((hop_timestamp_t*)&src[i], ((hop_timestamp_t*) &src[i]) + count, std::back_inserter(entries.ends));
      i += sizeof( hop_timestamp_t ) * count;
   }

   {  // starts
      std::copy((hop_timestamp_t*)&src[i], ((hop_timestamp_t*)&src[i]) + count, std::back_inserter(entries.starts));
      i += sizeof( hop_timestamp_t ) * count;
   }

   {  // depths
      std::copy((hop_depth_t*)&src[i], ((hop_depth_t*) &src[i]) + count, std::back_inserter(entries.depths));
      i += sizeof( hop_depth_t ) * count;
   }

   return i;
}

size_t serializedSize( const TraceData& td )
{
   const size_t tracesCount = td.entries.ends.size();
   const size_t size =
       sizeof( size_t ) +                           // Traces count
       serializedSize( td.entries ) +               // Entries size
       sizeof( hop_str_ptr_t ) * tracesCount * 2 +  // fileNameId and fctNameIds
       sizeof( hop_linenb_t ) * tracesCount +      // lineNbs
       sizeof( hop_zone_t ) * tracesCount;       // zones

   return size;
}

size_t serialize( const TraceData& td, char* dst )
{
   size_t i = 0;

   // Traces count
   const size_t tracesCount = td.entries.ends.size();
   memcpy( &dst[i], &tracesCount, sizeof( size_t ) );
   i += sizeof( size_t );

   // Entries
   i += serialize( td.entries, &dst[i] );

   // fileNameIds
   {
      std::copy( td.fileNameIds.begin(), td.fileNameIds.end(), (hop_str_ptr_t*)&dst[i] );
      i += sizeof( hop_str_ptr_t ) * tracesCount;
   }

   // fctNameIds
   {
      std::copy( td.fctNameIds.begin(), td.fctNameIds.end(), (hop_str_ptr_t*)&dst[i] );
      i += sizeof( hop_str_ptr_t ) * tracesCount;
   }

   // lineNbs
   {
      std::copy( td.lineNbs.begin(), td.lineNbs.end(), (hop_linenb_t*)&dst[i] );
      i += sizeof( hop_linenb_t ) * tracesCount;
   }

   // zones
   {
      std::copy( td.zones.begin(), td.zones.end(), (hop_zone_t*)&dst[i] );
      i += sizeof( hop_zone_t ) * tracesCount;
   }

   return i;
}

size_t deserialize( const char* src, TraceData& td )
{
   size_t i = 0;

   // Traces count
   const size_t tracesCount = *(size_t*)&src[i];
   i += sizeof( size_t );

   // Entries
   i += deserialize( &src[i], tracesCount, td.entries );

   {  // fileNames
      std::copy((hop_str_ptr_t*)&src[i], ((hop_str_ptr_t*)&src[i]) + tracesCount, std::back_inserter(td.fileNameIds));
      i += sizeof( hop_str_ptr_t ) * tracesCount;
   }

   {  // fctnames
      std::copy((hop_str_ptr_t*) &src[i], ((hop_str_ptr_t*)&src[i]) + tracesCount, std::back_inserter(td.fctNameIds));
      i += sizeof( hop_str_ptr_t ) * tracesCount;
   }

   {  // lineNbs
      std::copy((hop_linenb_t*)&src[i], ((hop_linenb_t*)&src[i]) + tracesCount, std::back_inserter(td.lineNbs));
      i += sizeof( hop_linenb_t ) * tracesCount;
   }

   {  // zones
      std::copy((hop_zone_t*)&src[i], ((hop_zone_t*)&src[i]) + tracesCount, std::back_inserter(td.zones));
      i += sizeof( hop_zone_t ) * tracesCount;
   }

   return i;
}

size_t serializedSize( const LockWaitData& lw )
{
   const size_t lockwaitsCount = lw.entries.ends.size();
   const size_t size = sizeof( size_t ) +                              // LockWaits count
                       serializedSize( lw.entries ) +                  // Entries size
                       sizeof( void* ) * lockwaitsCount +              // mutexAddrs
                       sizeof( hop_timestamp_t ) * lockwaitsCount;      // lockReleases
   return size;
}

size_t serialize( const LockWaitData& lw, char* dst )
{
   const size_t lockwaitsCount = lw.entries.ends.size();

   size_t i = 0;

   memcpy( &dst[i], &lockwaitsCount, sizeof( size_t ) );
   i += sizeof( size_t );

   // Entries
   i += serialize( lw.entries, &dst[i] );

   // mutexAddrs
   std::copy( lw.mutexAddrs.begin(), lw.mutexAddrs.end(), (void**)&dst[i] );
   i += sizeof( void* ) * lockwaitsCount;

   // lockReleases
   std::copy( lw.lockReleases.begin(), lw.lockReleases.end(), (hop_timestamp_t*)&dst[i] );
   i += sizeof( hop_timestamp_t ) * lockwaitsCount;

   return i;
}

size_t deserialize( const char* src, LockWaitData& lw )
{
   size_t i = 0;

   const size_t lwCounts = *(size_t*)&src[i];
   i += sizeof( size_t );

   // Entries
   i += deserialize( &src[i], lwCounts, lw.entries );

   // mutexAddrs
   std::copy((void**)&src[i], ((void**) &src[i]) + lwCounts, std::back_inserter(lw.mutexAddrs));
   i += sizeof( void* ) * lwCounts;

   // lockReleases
   std::copy((hop_timestamp_t*)&src[i], ((hop_timestamp_t*) &src[i]) + lwCounts, std::back_inserter(lw.lockReleases));
   i += sizeof( hop_timestamp_t ) * lwCounts;

   return i;
}

size_t serializedSize( const CoreEventData& ced )
{
   return sizeof( size_t ) +                              // CoreEvents count
          serializedSize( ced.entries ) +                 // Entries
          ced.cores.size() * sizeof( ced.cores[0] );      // Core information
}

size_t serialize( const CoreEventData& ced, char* dst )
{
   size_t i = 0;

   const size_t coreEventCount = ced.cores.size();

   memcpy( &dst[i], &coreEventCount, sizeof( size_t ) );
   i += sizeof( size_t );

   // Entries
   i += serialize( ced.entries, &dst[i] );

   // Core Events
   std::copy( ced.cores.begin(), ced.cores.end(), (hop_core_t*)&dst[i] );
   i += sizeof( ced.cores[0] ) * coreEventCount;

   return i;
}

size_t deserialize( const char* src, CoreEventData& ced )
{
   size_t i = 0;

   const size_t count = *(size_t*)&src[i];
   i += sizeof( size_t );

   i += deserialize( &src[i], count, ced.entries );

   // Core Events
   std::copy((hop_core_t*)&src[i], ((hop_core_t*) &src[i]) + count, std::back_inserter(ced.cores));
   i += sizeof( ced.cores[0] ) * count;

   return i;
}

} // namespace hop