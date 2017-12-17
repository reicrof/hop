#ifndef VDBG_MESSAGE_H_
#define VDBG_MESSAGE_H_

#include <stdint.h>

namespace vdbg
{

static constexpr const char* SERVER_PATH = "/tmp/my_server";

enum class MsgType : uint32_t
{
   EXIT = 0,
   MSG_1,
   INVALID_MESSAGE,
};

struct MsgHeader
{
   // Type of the message sent
   MsgType type;
   // Size of the message, including header
   uint32_t size;
};

}  // namespace vdbg

#endif  // VDBG_MESSAGE_H_