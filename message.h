#ifndef VDBG_MESSAGE_H_
#define VDBG_MESSAGE_H_

#include <stdint.h>
#include <chrono>

#include <platform.h>

namespace vdbg
{
static constexpr const char* SERVER_PATH = "/tmp/my_server";

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
using Precision = std::chrono::microseconds;
inline auto getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

// Right now, I expect the trace to be 64 bytes. The name of the function should fit inside the rest
// of the 64 bytes that is not used. It could (and should) be optimized later so we
// do not send all of the null character on the socket
constexpr uint32_t EXPECTED_TRACE_SIZE = 64;
static constexpr const uint32_t MAX_FCT_NAME_LENGTH =
    EXPECTED_TRACE_SIZE - sizeof( unsigned char ) - 2 * sizeof( TimeStamp );

struct TracesInfo
{
   uint32_t threadId;
   uint32_t traceCount;
};
VDBG_STATIC_ASSERT( sizeof(TracesInfo) == 8, "TracesInfo layout has changed unexpectedly" );

struct Trace
{
   TimeStamp start, end;
   unsigned char group;
   char name[MAX_FCT_NAME_LENGTH];
};
VDBG_STATIC_ASSERT( sizeof(Trace) == EXPECTED_TRACE_SIZE, "Trace layout has changed unexpectedly" );

}  // namespace vdbg

#endif  // VDBG_MESSAGE_H_