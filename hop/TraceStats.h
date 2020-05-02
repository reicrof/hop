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
   TraceDetail( size_t traceId, hop_timestamp_t excTime )
       : inclusiveTimeInNanos( 0 ),
         exclusiveTimeInNanos( excTime ),
         inclusivePct( 1.0f ),
         exclusivePct( 1.0f )
   {
      traceIds.reserve( 32 );
      traceIds.push_back( traceId );
   }
   std::vector< size_t > traceIds;
   hop_timestamp_t inclusiveTimeInNanos;
   hop_timestamp_t exclusiveTimeInNanos;
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
   hop_str_ptr_t fctNameId;
   size_t count;
   hop_timeduration_t min, max, median;
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
    bool drawAsCycles,
    float cpuFreqGHz );


TraceStats createTraceStats( const TraceData& traces, uint32_t threadIndex, size_t traceId );
void drawTraceStats( TraceStats& stats, const StringDb& strDb, bool drawAsCycles, float cpuFreqGHz );
void clearTraceDetails( TraceDetails& details );
void clearTraceStats( TraceStats& stats );
}

#endif  // TRACE_STATS_H_