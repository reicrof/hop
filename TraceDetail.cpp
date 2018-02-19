#include "TraceDetail.h"
#include "StringDb.h"
#include "Utils.h"

#include "imgui\imgui.h"

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
				traceDetails[index].deltaTimeInNanos += traceDelta;
			}
		}
		
		// Adjust the percentage
		float totalPct = 0.0f;
		for (auto& t : traceDetails)
		{
			t.exclusivePct = (double)t.deltaTimeInNanos / (double)totalDelta;
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

	bool drawTraceDetails(
		const TraceDetails& details,
		const std::vector<ThreadInfo>& tracesPerThread,
		const StringDb& strDb)
	{
		bool isWindowOpen = details.details.size() > 0;
		if (details.details.size() > 0 )
		{
			if (ImGui::Begin("Trace Details", &isWindowOpen))
			{
				const auto& threadInfo = tracesPerThread[details.threadIndex];

				ImGui::Columns(4, "TraceDetailsTable"); // 4-ways, with border
				ImGui::Separator();
				ImGui::Text("Trace"); ImGui::NextColumn();
				ImGui::Text("Excl. %%"); ImGui::NextColumn();
				ImGui::Text("Excl Time"); ImGui::NextColumn();
				ImGui::Text("Count"); ImGui::NextColumn();
				ImGui::Separator();

				char traceName[256] = {};
				char traceDuration[128] = {};
				static size_t selected = -1;
				for (size_t i = 0; i < details.details.size(); ++i)
				{
					const size_t traceId = details.details[i].traceId;
					TStrPtr_t classIdx = threadInfo.traces.classNameIds[traceId];
					TStrPtr_t fctIdx = threadInfo.traces.fctNameIds[traceId];
					strDb.formatTraceName(classIdx, fctIdx, traceName, sizeof(traceName));
					if (ImGui::Selectable(traceName, selected == i, ImGuiSelectableFlags_SpanAllColumns))
						selected = i;
					ImGui::NextColumn();
					ImGui::Text("%3.2f", details.details[i].exclusivePct * 100.0f); ImGui::NextColumn();
					formatMicrosDurationToDisplay( details.details[i].deltaTimeInNanos * 0.001f, traceDuration, sizeof(traceDuration) );
					ImGui::Text("%s", traceDuration); ImGui::NextColumn();
					ImGui::Text("%zu", details.details[i].callCount); ImGui::NextColumn();
				}
				ImGui::Columns(1);
			}
			ImGui::End();
		}

		return isWindowOpen;
	}

}