#ifndef TRACE_STATS_H_
#define TRACE_STATS_H_

#include "Hop.h"
#include <vector>

namespace hop
{
class StringDb;
struct TraceData;
struct TimelineTrack;

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
   bool open{false};
   bool shouldFocusWindow{false};
};

struct TraceDetailDrawResult
{
   std::vector< size_t > hoveredTraceIds;
   uint32_t hoveredThreadIdx;
   bool clicked;
};

struct TraceStats
{
   StrPtr_t fctNameId;
   size_t count;
   TimeDuration min, max, median;
   std::vector< float > displayableDurations;
   bool open{false};
   bool focus{false};
};

TraceDetails
createTraceDetails( const TraceData& traces, uint32_t threadIndex, size_t traceId );
TraceDetails createGlobalTraceDetails( const TraceData& traces, uint32_t threadIndex );
TraceDetailDrawResult drawTraceDetails(
    TraceDetails& details,
    const std::vector<TimelineTrack>& tracks,
    const StringDb& strDb,
    bool drawAsCycles );


TraceStats createTraceStats( const TraceData& traces, uint32_t threadIndex, size_t traceId );
void drawTraceStats( TraceStats& stats, const StringDb& strDb, bool drawAsCycles );
void clearTraceDetails( TraceDetails& details );
void clearTraceStats( TraceStats& stats );
}

#endif  // TRACE_STATS_H_