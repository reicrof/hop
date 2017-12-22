#ifndef VDBG_MESSAGE_H_
#define VDBG_MESSAGE_H_

#include <stdint.h>
#include <chrono>

#include <platform.h>

namespace vdbg
{
static VDBG_CONSTEXPR const char* SERVER_PATH = "/tmp/my_server";

enum class MsgType : uint32_t
{
   PROFILER_TRACE,
   INVALID_MESSAGE,
};

struct MsgHeader
{
   // Type of the message sent
   MsgType type;
   // Size of the message
   uint32_t size;
};

using Clock = std::chrono::high_resolution_clock;
using Precision = std::chrono::nanoseconds;
inline auto getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

VDBG_CONSTEXPR uint32_t EXPECTED_TRACE_INFO_SIZE = 16;
struct TracesInfo
{
   uint32_t threadId;
   uint32_t stringDataSize;
   uint32_t traceCount;
   uint32_t padding;
};
VDBG_STATIC_ASSERT(
    sizeof( TracesInfo ) == EXPECTED_TRACE_INFO_SIZE,
    "TracesInfo layout has changed unexpectedly" );

VDBG_CONSTEXPR uint32_t EXPECTED_TRACE_SIZE = 32;
struct Trace
{
   TimeStamp start, end;  // Timestamp for start/end of this trace
   uint32_t classNameIdx; // Index into string array for class name
   uint32_t fctNameIdx;   // Index into string array for function name
   uint32_t group;        // Group to which this trace belongs
   uint32_t padding;      // extra dummy padding...
};
VDBG_STATIC_ASSERT( sizeof(Trace) == EXPECTED_TRACE_SIZE, "Trace layout has changed unexpectedly" );

}  // namespace vdbg

#endif  // VDBG_MESSAGE_H_