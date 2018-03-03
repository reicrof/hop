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
		size_t traceId;
		TimeStamp deltaTimeInNanos;
		float exclusivePct;
		uint32_t callCount;
	};

	struct TraceDetails
	{
		std::vector<TraceDetail> details;
		uint32_t threadIndex;
        bool shouldFocusWindow{ false };
	};

	TraceDetails createTraceDetails(const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId);
	bool drawTraceDetails(
		const TraceDetails& details,
		const std::vector<ThreadInfo>& tracesPerThread,
		const StringDb& strDb);

}

#endif // TRACE_DETAIL_H_