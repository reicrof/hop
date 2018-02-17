#include "TraceDetail.h"
#include "DisplayableTraces.h"

#include <algorithm>

#include <cassert>

namespace hop
{

	TraceDetails createTraceDetails(const DisplayableTraces& traces, uint32_t threadIndex, size_t traceId)
	{
		// Find the traces to analyze
		const TimeStamp end = traces.ends[traceId];
		const TimeStamp totalDelta = traces.deltas[traceId];

		size_t firstTraceId = traceId;
		const TimeStamp firstTraceTime = end - totalDelta;
		while (firstTraceId > 0 && traces.ends[firstTraceId] >= firstTraceTime)
			firstTraceId--;

		++firstTraceId;

		std::vector< TraceDetail > traceDetails;
		std::vector< std::pair< TLineNb_t, TStrPtr_t>  > uniqueTraces;
		traceDetails.reserve(traceId - firstTraceId);
		uniqueTraces.reserve(traceId - firstTraceId);

		const TDepth_t maxDepth = *std::max_element(traces.depths.begin() + firstTraceId, traces.depths.begin() + traceId + 1);
		std::vector< TimeStamp > accumulatedTimePerDepth(maxDepth + 1, 0);

		TDepth_t lastDepth = traces.depths[firstTraceId];
		for (size_t i = firstTraceId; i <= traceId; ++i)
		{
			TimeStamp traceDelta = traces.deltas[i];
			const TDepth_t curDepth = traces.depths[i];

			if (curDepth == lastDepth)
			{
				accumulatedTimePerDepth[curDepth] += traceDelta;
			}
			else if (curDepth > lastDepth)
			{
				for (auto i = lastDepth + 1; i < curDepth; ++i)
					accumulatedTimePerDepth[i] = 0;

				accumulatedTimePerDepth[curDepth] = traceDelta;
			}
			else if (curDepth < lastDepth)
			{
				traceDelta -= accumulatedTimePerDepth[lastDepth];
				accumulatedTimePerDepth[curDepth] += traces.deltas[i];
			}

			lastDepth = curDepth;

			const auto traceId = std::make_pair(traces.lineNbs[i], traces.fileNameIds[i]);
			auto it = std::find(uniqueTraces.begin(), uniqueTraces.end(), traceId);
			if (it == uniqueTraces.end())
			{
				// New entry
				uniqueTraces.emplace_back(traceId);
				traceDetails.emplace_back(TraceDetail{i,traceDelta,1.0f,1});
			}
			else
			{
				size_t index = std::distance(uniqueTraces.begin(), it);
				++traceDetails[index].callCount;
				traceDetails[index].deltaTimeInMicros += traceDelta;
			}
		}
		
		// Adjust the percentage
		float totalPct = 0.0f;
		for (auto& t : traceDetails)
		{
			t.exclusivePct = (double)t.deltaTimeInMicros / (double)totalDelta;
			totalPct += t.exclusivePct;
		}

		// Sort them by exclusive percent
		std::sort(traceDetails.begin(), traceDetails.end(), [](const TraceDetail& lhs, const TraceDetail& rhs)
		{
			return lhs.exclusivePct > rhs.exclusivePct;
		});

		assert(std::abs(totalPct - 1.0f) < 0.01);

		TraceDetails details;
		details.threadIndex = threadIndex;
		std::swap(details.details, traceDetails);
		return details;
	}

}