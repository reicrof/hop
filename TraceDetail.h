#ifndef TRACE_DETAIL_H_
#define TRACE_DETAIL_H_

#include "Hop.h"

#include <vector>

namespace hop
{
	struct DisplayableTraces;
	struct TraceDetail
	{
		size_t traceId;
		TimeStamp deltaTimeInMicros;
		float exclusivePct;
		uint32_t callCount;
	};

	struct TraceDetails
	{
		std::vector<TraceDetail> details;
		uint32_t threadIndex;
	};

	TraceDetails createTraceDetails(const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId);

}

#endif // TRACE_DETAIL_H_