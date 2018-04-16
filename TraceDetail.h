#ifndef TRACE_DETAIL_H_
#define TRACE_DETAIL_H_

#include "Hop.h"
#include "ThreadInfo.h"

#include <vector>

namespace hop
{
class StringDb;
struct DisplayableTraces;
struct TraceDetail
{
   TraceDetail( size_t traceId, TimeStamp excTime )
       : inclusiveTimeInNanos( 0 ),
         exclusiveTimeInNanos( excTime ),
         inclusivePct( 1.0f ),
         exclusivePct( 1.0f )
   {
      traceIds.reserve( 32 );
      traceIds.push_back( traceId );
   }
   std::vector< size_t > traceIds;
   TimeStamp inclusiveTimeInNanos;
   TimeStamp exclusiveTimeInNanos;
   float inclusivePct;
   float exclusivePct;
};

struct TraceDetails
{
   std::vector<TraceDetail> details;
   uint32_t threadIndex;
   bool shouldFocusWindow{false};
};

struct TraceDetailDrawResult
{
   std::vector< size_t > hoveredTraceIds;
   uint32_t hoveredThreadIdx;
   bool isWindowOpen;
};

TraceDetails
createTraceDetails( const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId );
TraceDetails createGlobalTraceDetails( const DisplayableTraces& traces, uint32_t threadIndex );
TraceDetailDrawResult drawTraceDetails(
    const TraceDetails& details,
    const std::vector<ThreadInfo>& tracesPerThread,
    const StringDb& strDb );
}

#endif  // TRACE_DETAIL_H_