#ifndef VDBG_MESSAGE_H_
#define VDBG_MESSAGE_H_

#include <stdint.h>
#include <chrono>

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
   // Size of the message, including header
   uint32_t size;
};

using Clock = std::chrono::high_resolution_clock;
using Precision = std::chrono::nanoseconds;
inline auto getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );
static constexpr const uint32_t MAX_FCT_NAME_LENGTH = 64 - 2 * sizeof( TimeStamp );
struct TraceInfo
{
   TimeStamp start, end;
   char name[MAX_FCT_NAME_LENGTH];
};

struct Traces
{
   uint8_t threadId;
   uint32_t traceCount;
};

}  // namespace vdbg

#endif  // VDBG_MESSAGE_H_