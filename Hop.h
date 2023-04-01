/*This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
#ifndef HOP_H_
#define HOP_H_

// You can disable completly HOP by setting this variable
// to false
#if !defined( HOP_ENABLED )

// Stubbing all profiling macros so they are disabled
// when HOP_ENABLED is false
#define HOP_PROF( x )
#define HOP_PROF_FUNC()
#define HOP_PROF_SPLIT( x )
#define HOP_PROF_DYN_NAME( x )
#define HOP_PROF_MUTEX_LOCK( x )
#define HOP_PROF_MUTEX_UNLOCK( x )
#define HOP_ZONE( x )
#define HOP_SET_THREAD_NAME( x )
#define HOP_SET_CLIENT_NAME( x )

#else  // We do want to profile

///////////////////////////////////////////////////////////////
/////       THESE ARE THE MACROS YOU CAN MODIFY     ///////////
///////////////////////////////////////////////////////////////

// Total maximum of thread being traced
#if !defined( HOP_MAX_THREAD_NB )
#define HOP_MAX_THREAD_NB 64
#endif

// Total size of the shared memory ring buffer. This does not
// include the meta-data size
#if !defined( HOP_SHARED_MEM_SIZE )
#define HOP_SHARED_MEM_SIZE 32000000
#endif

// Minimum cycles for a lock to be considered in the profiled data
#if !defined( HOP_MIN_LOCK_CYCLES )
#define HOP_MIN_LOCK_CYCLES 1000
#endif

// Disable remote profiler by default
#if !defined( HOP_USE_REMOTE_PROFILER )
#define HOP_USE_REMOTE_PROFILER 1
#endif

// By default HOP will use a call to RDTSCP to get the current timestamp of the
// CPU. A mismatch in synchronization was noted on some machine having multiple
// physical CPUs. This would show up in the viewer as infinitly long traces or
// traces that overlaps each-other. The std::chrono library seems to handle
// this use case correctly. You can therefore enable the use of std::chrono
// by setting this variable to 1. This will also increase the over-head of
// HOP in your application.
#ifndef HOP_USE_STD_CHRONO
#if defined(_M_X64) || defined(__x86_64__)
  #define HOP_USE_STD_CHRONO 0
 #else
  #define HOP_USE_STD_CHRONO 1
 #endif
#endif

///////////////////////////////////////////////////////////////
/////       THESE ARE THE MACROS YOU SHOULD USE     ///////////
///////////////////////////////////////////////////////////////

// Create a new profiling trace with specified name. Name must be static
#define HOP_PROF( x ) HOP_PROF_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, ( x ) ) )

// Create a new profiling trace with the compiler provided name
#define HOP_PROF_FUNC() HOP_PROF_ID_GUARD( hop__, ( __FILE__, __LINE__, HOP_FCT_NAME ) )

// Split a profiling trace with a new provided name. Name must be static.
#define HOP_PROF_SPLIT( x ) HOP_PROF_ID_SPLIT( hop__, ( __FILE__, __LINE__, ( x ) ) )

// Create a new profiling trace for dynamic strings. Please use sparingly as they will incur more
// slowdown
#define HOP_PROF_DYN_NAME( x ) \
   HOP_PROF_DYN_STRING_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, ( x ) ) )

// Create a trace that represent the time waiting for a mutex. You need to provide
// a pointer to the mutex that is being locked
#define HOP_PROF_MUTEX_LOCK( x ) HOP_MUTEX_LOCK_GUARD_VAR( __LINE__, ( x ) )

// Create an event that correspond to the unlock of the specified mutex. This is
// used to provide stall region. You should provide a pointer to the mutex that
// is being unlocked.
#define HOP_PROF_MUTEX_UNLOCK( x ) HOP_MUTEX_UNLOCK_EVENT( x )

// The zone id specified must be between 0-255, where the default zone is 0
#define HOP_ZONE( x ) HOP_ZONE_GUARD( __LINE__, ( x ) )

// Set the name of the current thread in the profiler. Only the first call will
// be considered for each thread.
#define HOP_SET_THREAD_NAME( x ) hop::ClientManager::SetThreadName( ( x ) )

// Set the name of the client app
#define HOP_SET_CLIENT_NAME( x ) hop::ClientManager::SetClientName( ( x ) )

#define HOP_SHUTDOWN() hop::ClientManager::Shutdown()


#if HOP_USE_REMOTE_PROFILER
#define HOP_DEFAULT_PORT "33435"
#endif


///////////////////////////////////////////////////////////////
/////     EVERYTHING AFTER THIS IS IMPL DETAILS        ////////
///////////////////////////////////////////////////////////////

/*
                              /NN\
                              :NN:
                           ..-+NN+-..
                      ./mmmNNNNNNNNNmmmm\.
                   .mmNNNNNNNNNNNNNNNNNNNmm.
                 .nNNNNNNNNNNNNNNNNNNNNNNNNNNn.
               .nmNNNNNNNNNNNNNNNNNNNNNNNNNNNNmn.
              .nNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNn.
              .oydmmdhs+:`+NNNNNNNNNNh.-+shdmmdy+.
                 .////shmd.-dNNNNNNN+.dmhs+/:/.
                `dNdmdNNdmo `+mdNhs` ommdNNNdmd`
                sNmdmmdmds`ss--sh--hs`ommNNNNNdo
                dNmNNNmd:.yNNmy--ymmNd.:hNNNmmmm
                dNNNds:-/`yNNNNmmNNNNh`/-:sdNNNd
                -:-./ohmms`omNNNNNNmo`smmho/.-:-
                   .mNNNNmh-:hNNNNh:-hmNNNNm.
                   `mmmmmmmd `+dd+` dmmmmmmm`
                    smmmmmd--d+--+d--dmmmmms
                    `hmmms-/dmmddmmd/-smmmh`
                     `o+. ommmmmmmmmmo .+o`
                          .ymmmmmmmmy.
                           `smmmmmms`
                             \dmmd/
                              -yy-
                               ``
                       | || |/ _ \| _ \
                       | __ | (_) |  _/
                       |_||_|\___/|_|
*/

// Useful macros
#define HOP_VERSION 0.93f
#define HOP_ZONE_MAX  255
#define HOP_ZONE_DEFAULT 0
#define HOP_CONSTEXPR constexpr
#define HOP_NOEXCEPT noexcept
#define HOP_STATIC_ASSERT static_assert

#ifdef __clang__
#define HOP_NO_DESTROY [[clang::no_destroy]]
#else
#define HOP_NO_DESTROY
#endif

#include <stdint.h>

#ifdef HOP_USE_STD_CHRONO
   #include <chrono>
#endif

/* Windows specific macros and defines */
#if defined( _MSC_VER )
#ifndef NOMINMAX
#define NOMINMAX
#endif

#if defined( _M_X64 )
#define HOP_ARCH_x86 1
#else
#define HOP_ARCH_x86 0
#endif

#if defined( HOP_IMPLEMENTATION )
#define HOP_API __declspec( dllexport )
#else
#define HOP_API
#endif

#include <tchar.h>
typedef void* shm_handle;  // HANDLE is a void*
typedef TCHAR HOP_CHAR;

#if !HOP_USE_STD_CHRONO
#include <intrin.h>        // __rdtscp
#endif

// Type defined in unistd.h
#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long
#endif  // _WIN64

#else /* Unix (Linux & MacOs) specific macros and defines */

#include <sys/types.h> // ssize_t
typedef int shm_handle;
typedef char HOP_CHAR;

#define HOP_API

#if defined( __x86_64__ )
#define HOP_ARCH_x86 1
#else
#define HOP_ARCH_x86 0
#endif

#endif

// -----------------------------
// Forward declarations of type used by ringbuffer as adapted from
// Mindaugas Rasiukevicius. See below for Copyright/Disclaimer
typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;
// -----------------------------

namespace hop
{
// Custom trace types
using TimeStamp    = uint64_t;
using TimeDuration = int64_t;
using StrPtr_t     = uint64_t;
using LineNb_t     = uint32_t;
using Core_t       = uint32_t;
using Depth_t      = uint16_t;
using ZoneId_t     = uint16_t;

#if !HOP_USE_STD_CHRONO
inline TimeStamp rdtscp( uint32_t& aux )
{
#if defined( _MSC_VER )
   return __rdtscp( &aux );
#else
   uint64_t rax, rdx;
   asm volatile( "rdtscp\n" : "=a"( rax ), "=d"( rdx ), "=c"( aux ) : : );
   return ( rdx << 32U ) + rax;
#endif
}
#endif

inline TimeStamp getTimeStamp( Core_t& core )
{
   // We return the timestamp with the first bit set to 0. We do not require this last cycle/nanosec
   // of precision. It will instead be used to flag if a trace uses dynamic strings or not in its
   // start time. See hop::StartProfileDynString
#if HOP_USE_STD_CHRONO
   using namespace std::chrono;
   core = 0;
   return (TimeStamp)duration_cast<nanoseconds>( steady_clock::now().time_since_epoch() ).count() &
          ~1ULL;
#else
   return hop::rdtscp( core ) & ~1ULL;
#endif
}

inline TimeStamp getTimeStamp()
{
   uint32_t dummyCore;
   return getTimeStamp( dummyCore );
}

enum class MsgType : uint32_t
{
   PROFILER_TRACE,
   PROFILER_STRING_DATA,
   PROFILER_WAIT_LOCK,
   PROFILER_UNLOCK_EVENT,
   PROFILER_HEARTBEAT,
   PROFILER_CORE_EVENT,
   PROFILER_HANDSHAKE,
   INVALID_MESSAGE,
};

struct TracesMsgInfo
{
   uint32_t count;
};

struct StringDataMsgInfo
{
   uint32_t size;
};

struct LockWaitsMsgInfo
{
   uint32_t count;
};

struct UnlockEventsMsgInfo
{
   uint32_t count;
};

struct CoreEventMsgInfo
{
   uint32_t count;
};

HOP_CONSTEXPR uint32_t EXPECTED_MSG_INFO_SIZE = 40;
struct MsgInfo
{
   MsgType type;
   // Thread id from which the msg was sent
   uint32_t threadIndex;
   uint32_t seed;
   uint64_t threadId;
   StrPtr_t threadName;
   // Specific message data
   union {
      TracesMsgInfo traces;
      StringDataMsgInfo stringData;
      LockWaitsMsgInfo lockwaits;
      UnlockEventsMsgInfo unlockEvents;
      CoreEventMsgInfo coreEvents;
   };
};
HOP_STATIC_ASSERT(
    sizeof( MsgInfo ) == EXPECTED_MSG_INFO_SIZE,
    "MsgInfo layout has changed unexpectedly" );

struct ViewerMsgInfo
{
   uint32_t seed;
   int8_t listening = -1;  // 0 false, 1 true, -1 no change
   int8_t connected = -1;  // 0 false, 1 true, -1 no change
   bool requestHandshake;
};

struct Traces
{
   uint32_t count;
   uint32_t maxSize;
   TimeStamp *starts, *ends;  // Timestamp for start/end of this trace
   StrPtr_t* fileNameIds;     // Index into string array for the file name
   StrPtr_t* fctNameIds;      // Index into string array for the function name
   LineNb_t* lineNumbers;     // Line at which the trace was inserted
   Depth_t* depths;           // The depth in the callstack of this trace
   ZoneId_t* zones;           // Zone to which this trace belongs
};

HOP_CONSTEXPR uint32_t EXPECTED_LOCK_WAIT_SIZE = 32;
struct LockWait
{
   void* mutexAddress;
   TimeStamp start, end;
   Depth_t depth;
   uint16_t padding;
};
HOP_STATIC_ASSERT(
    sizeof( LockWait ) == EXPECTED_LOCK_WAIT_SIZE,
    "Lock wait layout has changed unexpectedly" );

HOP_CONSTEXPR uint32_t EXPECTED_UNLOCK_EVENT_SIZE = 16;
struct UnlockEvent
{
   void* mutexAddress;
   TimeStamp time;
};
HOP_STATIC_ASSERT(
    sizeof( UnlockEvent ) == EXPECTED_UNLOCK_EVENT_SIZE,
    "Unlock Event layout has changed unexpectedly" );

struct CoreEvent
{
   TimeStamp start, end;
   Core_t core;
};

struct NetworkHandshake
{
   uint32_t pid;
   uint32_t maxThreadCount;
   uint64_t sharedMemSize;
   float cpuFreqGhz;
   TimeStamp connectionTs;
   char appName[64];
};

struct NetworkHandshakeMsgInfo
{
   MsgInfo info;
   NetworkHandshake handshake;
};

struct NetworkCompressionHeader
{
    uint32_t canary;
    uint32_t compressed;
    size_t compressedSize;
    size_t uncompressedSize;
};

class Client;
class SharedMemory;
class NetworkConnection;

class HOP_API ClientManager
{
  public:
   static Client* Get();
   static ZoneId_t StartProfile();
   static StrPtr_t StartProfileDynString( const char*, ZoneId_t* );
   static void EndProfile(
       StrPtr_t fileName,
       StrPtr_t fctName,
       TimeStamp start,
       TimeStamp end,
       LineNb_t lineNb,
       ZoneId_t zone,
       Core_t core );
   static void EndLockWait( void* mutexAddr, TimeStamp start, TimeStamp end );
   static void UnlockEvent( void* mutexAddr, TimeStamp time );
   static void SetThreadName( const char* name ) HOP_NOEXCEPT;
   static void SetClientName( const char* name ) HOP_NOEXCEPT;
   static ZoneId_t PushNewZone( ZoneId_t newZone );
   static bool HasConnectedConsumer() HOP_NOEXCEPT;
   static bool HasListeningConsumer() HOP_NOEXCEPT;
   static bool ShouldSendHeartbeat( TimeStamp curTimestamp ) HOP_NOEXCEPT;
   static void SetLastHeartbeatTimestamp( TimeStamp t ) HOP_NOEXCEPT;

   static SharedMemory& sharedMemory() HOP_NOEXCEPT;
   static NetworkConnection& networkConnection() HOP_NOEXCEPT;

   static void Shutdown() HOP_NOEXCEPT;
};

class ProfGuard
{
  public:
   ProfGuard( const char* fileName, LineNb_t lineNb, const char* fctName ) HOP_NOEXCEPT
   {
      open( fileName, lineNb, fctName );
   }
   ~ProfGuard() { close(); }
   inline void reset( const char* fileName, LineNb_t lineNb, const char* fctName )
   {
      // Please uncomment the following line if close() is made public!
      // if ( _fctName )
      close();
      open( fileName, lineNb, fctName );
   }

  private:
   inline void open( const char* fileName, LineNb_t lineNb, const char* fctName )
   {
      _start    = getTimeStamp();
      _fileName = reinterpret_cast<StrPtr_t>( fileName );
      _fctName  = reinterpret_cast<StrPtr_t>( fctName );
      _lineNb   = lineNb;
      _zone     = ClientManager::StartProfile();
   }
   inline void close()
   {
      uint32_t core;
      const auto end = getTimeStamp( core );
      ClientManager::EndProfile( _fileName, _fctName, _start, end, _lineNb, _zone, core );
      // Please uncomment the following line if close() is made public!
      // _fctName = nullptr;
   }

   TimeStamp _start;
   StrPtr_t _fileName, _fctName;
   LineNb_t _lineNb;
   ZoneId_t _zone;
};

class LockWaitGuard
{
   TimeStamp start;
   void* mutexAddr;
  public:
   LockWaitGuard( void* mutAddr ) : start( getTimeStamp() ), mutexAddr( mutAddr ) {}
   ~LockWaitGuard() { ClientManager::EndLockWait( mutexAddr, start, getTimeStamp() ); }
};

class ProfGuardDynamicString
{
  public:
   ProfGuardDynamicString( const char* fileName, LineNb_t lineNb, const char* fctName ) HOP_NOEXCEPT
       : _start( getTimeStamp() | 1ULL ),  // Set the first bit to 1 to signal dynamic strings
         _fileName( reinterpret_cast<StrPtr_t>( fileName ) ),
         _lineNb( lineNb )
   {
      _fctName = ClientManager::StartProfileDynString( fctName, &_zone );
   }
   ~ProfGuardDynamicString()
   {
      ClientManager::EndProfile( _fileName, _fctName, _start, getTimeStamp(), _lineNb, _zone, 0 );
   }

  private:
   TimeStamp _start;
   StrPtr_t _fileName;
   StrPtr_t _fctName;
   LineNb_t _lineNb;
   ZoneId_t _zone;
};

class ZoneGuard
{
  public:
   ZoneGuard( ZoneId_t newZone ) HOP_NOEXCEPT
   {
      _prevZoneId = ClientManager::PushNewZone( newZone );
   }
   ~ZoneGuard() { ClientManager::PushNewZone( _prevZoneId ); }

  private:
   ZoneId_t _prevZoneId;
};

void sleepMs( uint32_t ms );

#define HOP_PROF_GUARD_VAR( LINE, ARGS ) hop::ProfGuard HOP_COMBINE( hopProfGuard, LINE ) ARGS
#define HOP_PROF_ID_GUARD( ID, ARGS ) hop::ProfGuard ID ARGS
#define HOP_PROF_ID_SPLIT( ID, ARGS ) ID.reset ARGS
#define HOP_PROF_DYN_STRING_GUARD_VAR( LINE, ARGS ) \
   hop::ProfGuardDynamicString HOP_COMBINE( hopProfGuard, LINE ) ARGS
#define HOP_MUTEX_LOCK_GUARD_VAR( LINE, ARGS ) \
   hop::LockWaitGuard HOP_COMBINE( hopMutexLock, LINE ) ARGS
#define HOP_MUTEX_UNLOCK_EVENT( x ) hop::ClientManager::UnlockEvent( x, hop::getTimeStamp() );
#define HOP_ZONE_GUARD( LINE, ARGS ) hop::ZoneGuard HOP_COMBINE( hopZoneGuard, LINE ) ARGS

#define HOP_COMBINE( X, Y ) X##Y
#if defined( _MSC_VER )
#define HOP_FCT_NAME __FUNCTION__
#else
#define HOP_FCT_NAME __PRETTY_FUNCTION__
#endif

}  // namespace hop

/*
 * Copyright (c) 2016 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
int ringbuf_setup( ringbuf_t*, unsigned, size_t );
void ringbuf_get_sizes( unsigned, size_t*, size_t* );

ringbuf_worker_t* ringbuf_register( ringbuf_t*, unsigned );
void ringbuf_unregister( ringbuf_t*, ringbuf_worker_t* );

ssize_t ringbuf_acquire( ringbuf_t*, ringbuf_worker_t*, size_t );
void ringbuf_produce( ringbuf_t*, ringbuf_worker_t* );
size_t ringbuf_consume( ringbuf_t*, size_t* );
void ringbuf_release( ringbuf_t*, size_t );
void ringbuf_clear( ringbuf_t* );

#if HOP_USE_REMOTE_PROFILER

/*
 *
 * LZJB Compression Implementation
 *
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */

size_t lzjb_compress(const void *s_start, void *d_start, size_t s_len, size_t d_len);
int lzjb_decompress(const void *s_start, void *d_start, size_t s_len, size_t d_len);

#endif

/* ======================================================================
                    End of public declarations
   ==================================================================== */

#if defined( HOP_VIEWER ) || defined( HOP_IMPLEMENTATION )
#include <atomic>
#include <mutex>

// On MacOs the max name length seems to be 30...
#define HOP_SHARED_MEM_MAX_NAME_SIZE 30
namespace hop
{
enum ConnectionState
{
   NO_TARGET_PROCESS,
   NOT_CONNECTED,
   CONNECTED,
   CONNECTED_NO_CLIENT,
   PERMISSION_DENIED,
   INVALID_VERSION,

   /* Network specific states */
   CANNOT_RESOLVE_ADDR,
   ADDR_ALREADY_IN_USE,
   WOULD_BLOCK,
   CANNOT_CONNECT_TO_SERVER,

   /* Lazy generic error */
   UNKNOWN_CONNECTION_ERROR
};

class SharedMemory
{
  public:
   ConnectionState create( int pid, size_t size, bool isConsumer );
   void destroy();

   struct SharedMetaInfo
   {
      enum Flags
      {
         CONNECTED_PRODUCER = 1 << 0,
         CONNECTED_CONSUMER = 1 << 1,
         LISTENING_CONSUMER = 1 << 2,
      };
      std::atomic<uint32_t> flags{ 0 };
      float clientVersion{ 0.0f };
      uint32_t maxThreadNb{ 0 };
      std::atomic<uint32_t> lastResetSeed{ 1 };
      size_t requestedSize{ 0 };
      std::atomic<TimeStamp> lastHeartbeatTimeStamp{ 0 };
      bool usingStdChronoTimeStamps{ false };
   };

   bool hasConnectedProducer() const HOP_NOEXCEPT;
   void setConnectedProducer( bool ) HOP_NOEXCEPT;
   bool hasConnectedConsumer() const HOP_NOEXCEPT;
   void setConnectedConsumer( bool ) HOP_NOEXCEPT;
   bool hasListeningConsumer() const HOP_NOEXCEPT;
   void setListeningConsumer( bool ) HOP_NOEXCEPT;
   bool shouldSendHeartbeat( TimeStamp t ) const HOP_NOEXCEPT;
   void setLastHeartbeatTimestamp( TimeStamp t ) HOP_NOEXCEPT;
   uint32_t lastResetSeed() const HOP_NOEXCEPT;
   void setResetSeed(uint32_t seed) HOP_NOEXCEPT;
   void reset() HOP_NOEXCEPT;
   ringbuf_t* ringbuffer() const HOP_NOEXCEPT;
   uint8_t* data() const HOP_NOEXCEPT;
   bool valid() const HOP_NOEXCEPT;
   int pid() const HOP_NOEXCEPT;
   const SharedMetaInfo* sharedMetaInfo() const HOP_NOEXCEPT;
   ~SharedMemory();

  private:
   // Pointer into the shared memory
   SharedMetaInfo* _sharedMetaData{ NULL };
   ringbuf_t* _ringbuf{ NULL };
   uint8_t* _data{ NULL };
   // ----------------
   bool _isConsumer{ false };
   shm_handle _sharedMemHandle{};
   int _pid;
   HOP_CHAR _sharedMemPath[HOP_SHARED_MEM_MAX_NAME_SIZE];
   std::atomic<bool> _valid{ false };
};

/* Internal viewer functions declaration */

float getCpuFreqGHz();

} //namespace hop

#if HOP_USE_REMOTE_PROFILER

#if defined( _MSC_VER )
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET Socket;
static const Socket HOP_NET_INVALID_SOCKET = INVALID_SOCKET;

static const int HOP_NET_WOULD_BLOCK  = WSAEWOULDBLOCK;
static const int HOP_NET_ALREADY_DONE = WSAEISCONN;
static const int HOP_NET_IN_PROGRESS  = WSAEALREADY;
static const int HOP_NET_ADDR_IN_USE  = WSAEADDRINUSE;
static const int HOP_NET_CONN_INVALID = WSAEADDRNOTAVAIL;
static const int HOP_NET_CONN_REFUSED = WSAECONNREFUSED;
static const int HOP_NET_ALREADY_CONN = WSAEISCONN;
static const int HOP_NET_CONN_RESET   = WSAECONNRESET;
static const int HOP_NET_SOCKET_ERROR = SOCKET_ERROR;
static const int HOP_NET_SHUT_SEND    = SD_SEND;
static inline int getLastNetworkError() { return WSAGetLastError(); }
static inline const char* hop_gai_strerror( int c ) {return gai_strerrorA( c );}
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>

typedef int Socket;
static const Socket HOP_NET_INVALID_SOCKET = -1;

static const int HOP_NET_WOULD_BLOCK  = EAGAIN;
static const int HOP_NET_ALREADY_DONE = EALREADY;
static const int HOP_NET_IN_PROGRESS  = EINPROGRESS;
static const int HOP_NET_ADDR_IN_USE  = EADDRINUSE;
static const int HOP_NET_CONN_REFUSED = ECONNREFUSED;
static const int HOP_NET_CONN_INVALID = ENETUNREACH;
static const int HOP_NET_ALREADY_CONN = EISCONN;
static const int HOP_NET_CONN_RESET   = -1;
static const int HOP_NET_SOCKET_ERROR = -1;
static const int HOP_NET_SHUT_SEND    = SHUT_WR;
static inline int getLastNetworkError() { return errno; };
static inline const char* hop_gai_strerror( int c ) {return gai_strerror( c );}
#endif

#include <thread>
typedef struct addrinfo Address;

namespace hop
{

class NetworkConnection
{
  public:
   static HOP_CONSTEXPR uint32_t ADDR_MAX_LEN    = 50;
   enum class Status
   {
      INVALID,
      ALIVE,
      PENDING_CONNECTION,
      ERROR_WOULD_BLOCK,
      ERROR_REFUSED,
      ERROR_INVALID_ADDR,
      ERROR_ADDR_IN_USE,
      ERROR_UNKNOWN,
      SHUTDOWN,
   };

   NetworkConnection() = default;
   NetworkConnection( NetworkConnection&& nc );
   bool operator==( const NetworkConnection& rhs ) const;
   ~NetworkConnection() { terminate (); }

   ConnectionState openConnection( bool isViewer );
   Status status() const HOP_NOEXCEPT { return _status; }

   bool start( SharedMemory& shmem );
   void stop();

   bool sendAllData( const void* data, size_t size, bool compresss );
   ssize_t receiveData( void* data, size_t size ) const;

   bool handshakeSent() const HOP_NOEXCEPT { return _handshakeSent; }
   void setHandshakeSent( bool sent ) HOP_NOEXCEPT { _handshakeSent = sent; }
   ConnectionState tryConnect();

   bool readReady()
   {
      struct timeval tv = { 0, 500 };
      FD_ZERO(&_socket_set);
      FD_SET(_clientSocket, &_socket_set);
      int res = select(FD_SETSIZE, &_socket_set, NULL, NULL, &tv);
      return res > 0;
   }

   void terminate();
   void reset();

   static char* errToStr( int err, char* buf, uint32_t len );

   char _portStr[16] = HOP_DEFAULT_PORT;
   char _addressStr[ADDR_MAX_LEN + 1] = {};
   uint8_t* _compressionBuffer        = nullptr;
   size_t _compressionBufferSz        = 0;

   std::thread _thread;
   Socket _socket                     = HOP_NET_INVALID_SOCKET;
   Socket _clientSocket               = HOP_NET_INVALID_SOCKET;
   Address* _addr                     = nullptr;
   Status _status                     = Status::INVALID;
   fd_set _socket_set;
   bool _handshakeSent                = false;
};

}  // namespace hop

#endif // HOP_USE_REMOTE_PROFILER
#endif // defined(HOP_VIEWER)

/* ======================================================================
                    End of private declarations
   ==================================================================== */

#if defined( HOP_IMPLEMENTATION )

// standard includes
#include <algorithm>
#include <cassert>
#include <memory>
#include <unordered_set>
#include <vector>

#define HOP_MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define HOP_MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define HOP_UNUSED( x ) (void)( x )

#if !defined( _MSC_VER )

// Unix shared memory includes
#include <fcntl.h>     // O_CREAT
#include <cstring>     // memcpy
#include <limits.h>    // CHAR_BIT
#include <pthread.h>   // pthread_self
#include <sys/mman.h>  // shm_open
#include <sys/stat.h>  // stat
#include <unistd.h>    // ftruncate

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = "/hop_";
#define HOP_STRLEN( str ) strlen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )

#define likely( x ) __builtin_expect( !!( x ), 1 )
#define unlikely( x ) __builtin_expect( !!( x ), 0 )

#define HOP_GET_THREAD_ID() reinterpret_cast<size_t>( pthread_self() )

inline int HOP_GET_PID() HOP_NOEXCEPT{ return getpid(); }

#else  // !defined( _MSC_VER )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = _T("/hop_");
#define HOP_STRLEN( str ) _tcslen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) _tcsncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) _tcsncat_s( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat_s( ( dst ), ( count ), ( src ), ( count ) )

#define likely( x ) ( x )
#define unlikely( x ) ( x )

#define HOP_GET_THREAD_ID() ( size_t ) GetCurrentThreadId()

inline int HOP_GET_PID() HOP_NOEXCEPT { return GetCurrentProcessId(); }

#endif  // !defined( _MSC_VER )

namespace
{
hop::ConnectionState errorToConnectionState( uint32_t err )
{
#if defined( _MSC_VER )
   if( err == ERROR_FILE_NOT_FOUND ) return hop::NOT_CONNECTED;
   if( err == ERROR_ACCESS_DENIED ) return hop::PERMISSION_DENIED;
   return hop::UNKNOWN_CONNECTION_ERROR;
#else
   if( err == ENOENT ) return hop::NOT_CONNECTED;
   if( err == EACCES ) return hop::PERMISSION_DENIED;
   return hop::UNKNOWN_CONNECTION_ERROR;
#endif
}

void* createSharedMemory(
    const HOP_CHAR* path,
    uint64_t size,
    shm_handle* handle,
    hop::ConnectionState* state )
{
   uint8_t* sharedMem{NULL};
#if defined( _MSC_VER )
   *handle = CreateFileMapping(
       INVALID_HANDLE_VALUE,  // use paging file
       NULL,                  // default security
       PAGE_READWRITE,        // read/write access
       size >> 32,            // maximum object size (high-order DWORD)
       size & 0xFFFFFFFF,     // maximum object size (low-order DWORD)
       path );                // name of mapping object

   if( *handle == NULL )
   {
      *state = errorToConnectionState( GetLastError() );
      return NULL;
   }
   sharedMem = (uint8_t*)MapViewOfFile(
       *handle,
       FILE_MAP_ALL_ACCESS,  // read/write permission
       0,
       0,
       size );

   if( sharedMem == NULL )
   {
      *state = errorToConnectionState( GetLastError() );
      CloseHandle( *handle );
      return NULL;
   }
#else
   *handle = shm_open( path, O_CREAT | O_RDWR, 0600 );
   if( *handle < 0 )
   {
      *state = errorToConnectionState( errno );
      return NULL;
   }

   int truncRes = ftruncate( *handle, size );
   if( truncRes != 0 )
   {
      *state = errorToConnectionState( errno );
      return NULL;
   }

   sharedMem = reinterpret_cast<uint8_t*>(
       mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 ) );
#endif
   if( sharedMem ) *state = hop::CONNECTED;
   return sharedMem;
}

void* openSharedMemory(
    const HOP_CHAR* path,
    shm_handle* handle,
    uint64_t* totalSize,
    hop::ConnectionState* state )
{
   uint8_t* sharedMem{NULL};
#if defined( _MSC_VER )
   *handle = OpenFileMapping(
       FILE_MAP_ALL_ACCESS,  // read/write access
       FALSE,                // do not inherit the name
       path );               // name of mapping object

   if( *handle == NULL )
   {
      *state = errorToConnectionState( GetLastError() );
      return NULL;
   }

   sharedMem = (uint8_t*)MapViewOfFile(
       *handle,
       FILE_MAP_ALL_ACCESS,  // read/write permission
       0,
       0,
       0 );

   if( sharedMem == NULL )
   {
      *state = errorToConnectionState( GetLastError() );
      CloseHandle( *handle );
      return NULL;
   }

   MEMORY_BASIC_INFORMATION memInfo;
   if( !VirtualQuery( sharedMem, &memInfo, sizeof( memInfo ) ) )
   {
      *state = errorToConnectionState( GetLastError() );
      UnmapViewOfFile( sharedMem );
      CloseHandle( *handle );
      return NULL;
   }
   *totalSize = memInfo.RegionSize;
#else
   *handle = shm_open( path, O_RDWR, 0600 );
   if( *handle < 0 )
   {
      *state = errorToConnectionState( errno );
      return NULL;
   }

   struct stat fileStat;
   if( fstat( *handle, &fileStat ) < 0 )
   {
      *state = errorToConnectionState( errno );
      return NULL;
   }

   *totalSize = fileStat.st_size;

   sharedMem = reinterpret_cast<uint8_t*>(
       mmap( NULL, fileStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 ) );
   *state = sharedMem ? hop::CONNECTED : hop::UNKNOWN_CONNECTION_ERROR;
#endif
   return sharedMem;
}

void closeSharedMemory( const HOP_CHAR* name, shm_handle handle, void* dataPtr )
{
#if defined( _MSC_VER )
   UnmapViewOfFile( dataPtr );
   CloseHandle( handle );
#else
   HOP_UNUSED( handle );  // Remove unuesed warning
   HOP_UNUSED( dataPtr );
   if( shm_unlink( name ) != 0 ) perror( " HOP - Could not unlink shared memory" );
#endif
}
}  // namespace

namespace hop
{
// The call stack depth of the current measured trace. One variable per thread
static thread_local int tl_traceLevel       = 0;
static thread_local uint32_t tl_threadIndex = 0;  // Index of the tread as they are coming in
static thread_local uint64_t tl_threadId    = 0;  // ID of the thread as seen by the OS
static thread_local ZoneId_t tl_zoneId      = HOP_ZONE_DEFAULT;
static thread_local char tl_threadNameBuffer[64];
static thread_local StrPtr_t tl_threadName  = 0;

static char g_clientNameBuffer[128];
static StrPtr_t g_clientName;
static std::atomic<bool> g_done{false};  // Was the shared memory destroyed? (Are we done?)

void sleepMs( uint32_t ms )
{
#if defined( _MSC_VER )
   Sleep( ms );
#else
   usleep( ms * 1000 );
#endif
}

static uint64_t alignOn( uint64_t val, uint64_t alignment );

ConnectionState
SharedMemory::create( int pid, size_t requestedSize, bool isConsumer )
{
   ConnectionState state = CONNECTED;

   // Create the shared data if it was not already created
   if( !_sharedMetaData )
   {
      _isConsumer = isConsumer;

      char pidStr[16];
      snprintf( pidStr, sizeof( pidStr ), "%d", pid );
      // Create shared mem name
      HOP_STRNCPYW(
          _sharedMemPath, HOP_SHARED_MEM_PREFIX, HOP_STRLEN( HOP_SHARED_MEM_PREFIX ) + 1 );
      HOP_STRNCATW(
          _sharedMemPath,
          pidStr,
          HOP_SHARED_MEM_MAX_NAME_SIZE - HOP_STRLEN( HOP_SHARED_MEM_PREFIX ) - 1 );

      // Try to open shared memory
      uint64_t totalSize = 0;
      uint8_t* sharedMem = reinterpret_cast<uint8_t*>(
          openSharedMemory( _sharedMemPath, &_sharedMemHandle, &totalSize, &state ) );

      // If we are the producer and we were not able to open the shared memory, we create it
      if( !isConsumer && !sharedMem )
      {
         size_t ringBufSize;
         ringbuf_get_sizes( HOP_MAX_THREAD_NB, &ringBufSize, NULL );
         totalSize = ringBufSize + requestedSize + sizeof( SharedMetaInfo );
         sharedMem = reinterpret_cast<uint8_t*>(
             createSharedMemory( _sharedMemPath, totalSize, &_sharedMemHandle, &state ) );
         if( sharedMem ) new( sharedMem ) SharedMetaInfo;  // Placement new for initializing values
      }

      if( !sharedMem )
      {
         return state;
      }

      SharedMetaInfo* metaInfo = reinterpret_cast<SharedMetaInfo*>( sharedMem );

      // Only the first producer setups the shared memory
      if( !isConsumer )
      {
         // Set client's info in the shared memory for the viewer to access
         metaInfo->clientVersion             = HOP_VERSION;
         metaInfo->maxThreadNb               = HOP_MAX_THREAD_NB;
         metaInfo->requestedSize             = HOP_SHARED_MEM_SIZE;
         metaInfo->usingStdChronoTimeStamps  = HOP_USE_STD_CHRONO;
         metaInfo->lastResetSeed             = 1;

         // Take a local copy as we do not want to expose the ring buffer before it is
         // actually initialized
         ringbuf_t* localRingBuf =
             reinterpret_cast<ringbuf_t*>( sharedMem + sizeof( SharedMetaInfo ) );

         // Then setup the ring buffer
         if( ringbuf_setup( localRingBuf, HOP_MAX_THREAD_NB, requestedSize ) < 0 )
         {
            assert( false && "Ring buffer creation failed" );
            closeSharedMemory( _sharedMemPath, _sharedMemHandle, sharedMem );
            return UNKNOWN_CONNECTION_ERROR;
         }
      }
      else  // Check if client has compatible version
      {
         if( std::abs( metaInfo->clientVersion - HOP_VERSION ) > 0.001f )
         {
            printf(
                "HOP - Client's version (%f) does not match HOP viewer version (%f)\n",
                static_cast<double>( metaInfo->clientVersion ),
                static_cast<double>( HOP_VERSION ) );
            destroy();
            return INVALID_VERSION;
         }
      }

      // Get the size needed for the ringbuf struct
      size_t ringBufSize;
      ringbuf_get_sizes( metaInfo->maxThreadNb, &ringBufSize, NULL );

      // Get pointers inside the shared memory once it has been initialized
      _sharedMetaData = reinterpret_cast<SharedMetaInfo*>( sharedMem );
      _ringbuf        = reinterpret_cast<ringbuf_t*>( sharedMem + sizeof( SharedMetaInfo ) );
      _data           = sharedMem + sizeof( SharedMetaInfo ) + ringBufSize;

      if( isConsumer )
      {
         // We can only have one consumer
         if( hasConnectedConsumer() )
         {
            printf(
                "/!\\ HOP WARNING /!\\ \n"
                "Cannot have more than one instance of the consumer at a time."
                " You might be trying to run the consumer application twice or"
                " have a dangling shared memory segment. hop might be unstable"
                " in this state. You could consider manually removing the shared"
                " memory, or restart this excutable cleanly.\n\n" );
            // Force resetting the listening state as this could cause crash. The side
            // effect would simply be that other consumer would stop listening. Not a
            // big deal as there should not be any other consumer...
            _sharedMetaData->flags &= ~( SharedMetaInfo::LISTENING_CONSUMER );
         }
      }

      isConsumer ? setConnectedConsumer( true ) : setConnectedProducer( true );
      _valid.store( true );
      _pid = pid;
   }

   return state;
}

bool SharedMemory::hasConnectedProducer() const HOP_NOEXCEPT
{
   return ( sharedMetaInfo()->flags & SharedMetaInfo::CONNECTED_PRODUCER ) > 0;
}

void SharedMemory::setConnectedProducer( bool connected ) HOP_NOEXCEPT
{
   if( connected )
      _sharedMetaData->flags |= SharedMetaInfo::CONNECTED_PRODUCER;
   else
      _sharedMetaData->flags &= ~SharedMetaInfo::CONNECTED_PRODUCER;
}

bool SharedMemory::hasConnectedConsumer() const HOP_NOEXCEPT
{
   return ( sharedMetaInfo()->flags & SharedMetaInfo::CONNECTED_CONSUMER ) > 0;
}

bool SharedMemory::shouldSendHeartbeat( TimeStamp curTimestamp ) const HOP_NOEXCEPT
{
   // When a profiled app is open, in the viewer but not listed to, we would spam
   // unnecessary heartbeats every time a trace stack was sent. This make sure we only
   // send them every few milliseconds
#if HOP_USE_STD_CHRONO
   static const uint64_t cyclesBetweenHB = 1e6;
#else
   static const uint64_t cyclesBetweenHB = 1e7;
#endif
   return curTimestamp - _sharedMetaData->lastHeartbeatTimeStamp.load() > cyclesBetweenHB;
}

void SharedMemory::setLastHeartbeatTimestamp( TimeStamp t ) HOP_NOEXCEPT
{
   _sharedMetaData->lastHeartbeatTimeStamp.store( t );
}

void SharedMemory::setConnectedConsumer( bool connected ) HOP_NOEXCEPT
{
   if( connected )
      _sharedMetaData->flags |= SharedMetaInfo::CONNECTED_CONSUMER;
   else
      _sharedMetaData->flags &= ~SharedMetaInfo::CONNECTED_CONSUMER;
}

bool SharedMemory::hasListeningConsumer() const HOP_NOEXCEPT
{
   const uint32_t mask = SharedMetaInfo::CONNECTED_CONSUMER | SharedMetaInfo::LISTENING_CONSUMER;
   return ( sharedMetaInfo()->flags.load() & mask ) == mask;
}

void SharedMemory::setListeningConsumer( bool listening ) HOP_NOEXCEPT
{
   if( listening )
      _sharedMetaData->flags |= SharedMetaInfo::LISTENING_CONSUMER;
   else
      _sharedMetaData->flags &= ~( SharedMetaInfo::LISTENING_CONSUMER );
}

uint32_t SharedMemory::lastResetSeed() const HOP_NOEXCEPT
{
   return _sharedMetaData->lastResetSeed.load();
}

void SharedMemory::setResetSeed( uint32_t seed ) HOP_NOEXCEPT
{
   _sharedMetaData->lastResetSeed.store( seed );
}

void SharedMemory::reset() HOP_NOEXCEPT
{
   uint32_t next_seed = lastResetSeed() + 1;
   setResetSeed( next_seed );
   setListeningConsumer( false );
   setConnectedConsumer( false );
   ringbuf_clear( _ringbuf );
}

uint8_t* SharedMemory::data() const HOP_NOEXCEPT { return _data; }

bool SharedMemory::valid() const HOP_NOEXCEPT { return _valid; }

int SharedMemory::pid() const HOP_NOEXCEPT { return _pid; }

ringbuf_t* SharedMemory::ringbuffer() const HOP_NOEXCEPT { return _ringbuf; }

const SharedMemory::SharedMetaInfo* SharedMemory::sharedMetaInfo() const HOP_NOEXCEPT
{
   return _sharedMetaData;
}

void SharedMemory::destroy()
{
   if( valid() )
   {
      if( _isConsumer )
      {
         setListeningConsumer( false );
         setConnectedConsumer( false );
      }
      else
      {
         setConnectedProducer( false );
      }

      // If we are the last one accessing the shared memory, clean it.
      if( ( _sharedMetaData->flags.load() &
            ( SharedMetaInfo::CONNECTED_PRODUCER | SharedMetaInfo::CONNECTED_CONSUMER ) ) == 0 )
      {
         printf( "HOP - Cleaning up shared memory...\n" );
         closeSharedMemory( _sharedMemPath, _sharedMemHandle, _sharedMetaData );
         _sharedMetaData->~SharedMetaInfo();
      }

      _data           = NULL;
      _ringbuf        = NULL;
      _sharedMetaData = NULL;
      _valid          = false;
   }
}

SharedMemory::~SharedMemory() { destroy(); }

/* Remote profiler implementation */

static float estimateCpuFreqHz()
{
#if !HOP_USE_STD_CHRONO
   using namespace std::chrono;
   uint32_t cpu;
   volatile uint64_t dummy = 0;
   // Do a quick warmup first
   for( int i = 0; i < 1000; ++i )
   {
      ++dummy;
      hop::rdtscp( cpu );
   }

   // Start timer and get current cycle count
   const auto startTime           = high_resolution_clock::now();
   const uint64_t startCycleCount = hop::rdtscp( cpu );

   // Make the cpu work hard
   for( int i = 0; i < 2000000; ++i )
   {
      dummy += i;
   }

   // Stop timer and get end cycle count
   const uint64_t endCycleCount = hop::rdtscp( cpu );
   const auto endTime           = high_resolution_clock::now();

   const uint64_t deltaCycles = endCycleCount - startCycleCount;
   const auto deltaTimeNs     = duration_cast<nanoseconds>( endTime - startTime );

   double countPerSec = duration<double>( seconds( 1 ) ) / deltaTimeNs;
   return deltaCycles * countPerSec;
#else
   return 2e9;
#endif
}

float getCpuFreqGHz()
{
   static float cpuFreq = 0;
   if( cpuFreq == 0 )
   {
      cpuFreq = estimateCpuFreqHz() / 1000000000.0;
   }

   return cpuFreq;
}

#if HOP_USE_REMOTE_PROFILER

static void networkThreadLoop( NetworkConnection& connection, SharedMemory& shmem );

NetworkConnection::NetworkConnection( NetworkConnection&& nc )
{
   memcpy( _portStr, nc._portStr, sizeof( _portStr ) );
   memcpy( _addressStr, nc._addressStr, sizeof( _addressStr ) );

   _thread        = std::move( nc._thread );
   _socket        = nc._socket;
   _clientSocket  = nc._clientSocket;
   _addr          = nc._addr;
   _status        = nc._status;
   _handshakeSent = nc._handshakeSent;

   nc._socket = nc._clientSocket = HOP_NET_INVALID_SOCKET;
   nc._addr = nullptr;

   _compressionBufferSz = 0;
   _compressionBuffer = nullptr;
}

bool NetworkConnection::operator==( const NetworkConnection& rhs ) const
{
   if (strcmp (_addressStr, rhs._addressStr) != 0)
      return false;

   if (strcmp (_portStr, rhs._portStr) != 0)
      return false;

   return true;
}

bool NetworkConnection::start( SharedMemory& shmem )
{
   _thread = std::thread( networkThreadLoop, std::ref(*this), std::ref(shmem));
   return true;
}

void NetworkConnection::stop()
{
   reset();
   _thread.join();
}

char* NetworkConnection::errToStr( int err, char* buf, uint32_t len )
{
#if defined( _MSC_VER )
   FormatMessage(
       FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL,
       err,
       MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
       (LPSTR)buf,
       len,
       NULL );
#else
   strerror_r( err, buf, len );
#endif
   return buf;
}

ConnectionState NetworkConnection::tryConnect() { return openConnection( false ); }

static void setSocketBlocking( Socket socket, bool blocking )
{
#ifdef _MSC_VER
      u_long iBlocking = blocking ? 0 : 1;  // 0 is blocking, 1 is non blocking
      const int res    = ioctlsocket( socket, FIONBIO, &iBlocking );
      if( res != NO_ERROR )
      {
         char msgBuffer[256];
         fprintf( stderr, "ioctlsocket failed: %s\n", NetworkConnection::errToStr( res, msgBuffer, 256 ) );
      }
#else
      const unsigned currentFlags = fcntl( socket, F_GETFL, 0 );
      if( (currentFlags & O_NONBLOCK) != !blocking )
      {
         unsigned newFlags = blocking ? currentFlags & ~O_NONBLOCK : currentFlags | O_NONBLOCK;
         fcntl( socket, F_SETFL, newFlags );
      }
#endif
}

ConnectionState NetworkConnection::openConnection( bool isViewer )
{
#if defined( _MSC_VER )
   static bool first = true;
   if( first )
   {
      struct WSAData wsData;
      if( WSAStartup( MAKEWORD( 2, 2 ), &wsData ) )
      {
         char msgBuffer[256];
         fprintf(
             stderr, "WSAStartup() failed : %s\n", errToStr( WSAGetLastError(), msgBuffer, 256 ) );
         return PERMISSION_DENIED;
      }
      first = false;
   }
#endif

   // Resolve the address
   if(!_addr && (_addressStr[0] || !isViewer) && _portStr[0] )
   {
      struct addrinfo hints = {};
      hints.ai_family       = AF_INET6;
      hints.ai_socktype     = SOCK_STREAM;
      hints.ai_protocol     = IPPROTO_TCP;
      hints.ai_flags        = isViewer ? 0 : AI_PASSIVE;

      int resolvedAddr = getaddrinfo( isViewer ? _addressStr : NULL, _portStr, &hints, &_addr );
      if( resolvedAddr != 0 )
      {
         fprintf( stderr, "Error resolving address: %s\n", hop_gai_strerror( resolvedAddr ) );
         reset();
         return CANNOT_RESOLVE_ADDR;
      }
   }

   // Open the socket
   if( _socket == HOP_NET_INVALID_SOCKET )
   {
      _socket = socket( _addr->ai_family, _addr->ai_socktype, _addr->ai_protocol );
      if( _socket == HOP_NET_INVALID_SOCKET )
      {
         reset();
         return CANNOT_RESOLVE_ADDR;
      }

      // Set the socket as non-blocking
      setSocketBlocking( _socket, false );

      // Start listening on the socket for a viewer if we are a client
      if( !isViewer )
      {
         const int bindRes = bind( _socket, _addr->ai_addr, (int)_addr->ai_addrlen );
         if( bindRes == HOP_NET_SOCKET_ERROR )
         {
            const int errval = getLastNetworkError();
            reset();
            if( errval != HOP_NET_ADDR_IN_USE )
            {
               char msgBuffer[256];
               fprintf(
                   stderr,
                   "bind failed error: %s\n",
                   errToStr( getLastNetworkError(), msgBuffer, 256 ) );
               return CANNOT_RESOLVE_ADDR;
            }
            else
            {
               _status = Status::ERROR_ADDR_IN_USE;
               return ADDR_ALREADY_IN_USE;
            }
         }

         if( listen( _socket, 1 ) == HOP_NET_SOCKET_ERROR )
         {
            char msgBuffer[256];
            fprintf(
                stderr,
                "Listen failed error: %s\n",
                errToStr( getLastNetworkError(), msgBuffer, 256 ) );
            reset();
            return CANNOT_RESOLVE_ADDR;
         }
      }
   }

   ConnectionState result = NOT_CONNECTED;

   // If we are the viewer, try to connect to the specified client
   if( isViewer )
   {
      const int connectRes = connect( _socket, _addr->ai_addr, (int)_addr->ai_addrlen );
      if( connectRes == HOP_NET_SOCKET_ERROR )
      {
         const int err = getLastNetworkError();

         if( err == HOP_NET_WOULD_BLOCK || err == HOP_NET_IN_PROGRESS)
         {
            // Connection request sent, but could not complete in time
            _status = Status::ERROR_WOULD_BLOCK;
            result  = WOULD_BLOCK;
         }
         else if( err == HOP_NET_CONN_REFUSED )
         {
            // Connection refused. Probably because target is not up
            _status = Status::ERROR_REFUSED;
            result  = CANNOT_CONNECT_TO_SERVER;
         }
         else if( err == HOP_NET_CONN_INVALID )
         {
            _status = Status::ERROR_INVALID_ADDR;
            result  = CANNOT_CONNECT_TO_SERVER;
         }
         else if( err == HOP_NET_ALREADY_DONE || err == HOP_NET_ALREADY_CONN )
         {
            // Already connected, nothing to be done
            result = CONNECTED;
         }
         else
         {
            // Unknown error
            char msgBuffer[256];
            fprintf( stderr, "Connect failed: %s\n", errToStr( err, msgBuffer, 256 ) );
            _status = Status::ERROR_UNKNOWN;
            result  = UNKNOWN_CONNECTION_ERROR;
         }
      }
      else
         result = CONNECTED;
      if( result == CONNECTED )
          _clientSocket = _socket;
   }
   else  // If we are the client, try accepting incoming viewer connection
   {
      int viewerSocket = -1;
      struct sockaddr_storage saddr;
      socklen_t sz = sizeof( saddr );
      while( viewerSocket == -1 )
      {
         viewerSocket = accept( _socket, (struct sockaddr*)&saddr, &sz );
         if( viewerSocket == -1 )
         {
            const int err = getLastNetworkError();
            if( err != HOP_NET_WOULD_BLOCK )
            {
               char msgBuffer[256];
               fprintf( stderr, "Accept failed  %s\n", errToStr( err, msgBuffer, 256 ) );
               break;
            }
         }
         hop::sleepMs( 500 );
      }
      _clientSocket = viewerSocket;
      result        = viewerSocket > 0 ? CONNECTED : NOT_CONNECTED;
   }

   if( result == CONNECTED)
   {
       _status = Status::ALIVE;
       if( !isViewer )
           setSocketBlocking( _clientSocket, true );
   }

   return result;
}

static void resetSocket( Socket* s )
{
   if( *s != HOP_NET_INVALID_SOCKET )
   {
      shutdown( *s, HOP_NET_SHUT_SEND );
#ifdef _MSC_VER
      closesocket( *s );
#else
      close( *s );
#endif
      *s = HOP_NET_INVALID_SOCKET;
   }
}

void NetworkConnection::reset()
{
   _status = Status::INVALID;
   resetSocket( &_socket );
   resetSocket( &_clientSocket );
   setHandshakeSent( false );

   if( _addr )
   {
      freeaddrinfo( _addr );
      _addr = nullptr;
   }
}

void NetworkConnection::terminate()
{
   reset();
   _status = Status::SHUTDOWN;
   free( _compressionBuffer );
   _compressionBuffer = nullptr;
   _compressionBufferSz = 0;
}

bool NetworkConnection::sendAllData( const void* data, size_t size, bool compress )
{
#ifdef _MSC_VER
   const int flags = 0;
#else
   const int flags = MSG_NOSIGNAL;
#endif

   uint8_t* dataToSend = (uint8_t*)data;
   size_t sizeToSend   = size;
   if( compress )
   {
      const size_t required_size = size + sizeof( NetworkCompressionHeader );
      if( required_size > _compressionBufferSz )
      {
         _compressionBufferSz = alignOn( required_size, (size_t)1024 );
         _compressionBuffer   = (uint8_t*) realloc( _compressionBuffer, _compressionBufferSz );
         assert( _compressionBuffer );
      }

      const bool do_compress = size > 512;
      dataToSend             = _compressionBuffer;

      NetworkCompressionHeader* header = (NetworkCompressionHeader*)dataToSend;
      uint8_t* dataDest                = _compressionBuffer + sizeof( NetworkCompressionHeader );
      header->canary                   = 0xBADC0FFE;
      header->compressed               = do_compress;
      header->uncompressedSize         = size;
      if( do_compress )
      {
         header->compressedSize = lzjb_compress( data, dataDest, size, size );
         assert( header->compressedSize > 0 );
      }
      else
      {
         header->compressedSize = size;
         memcpy( dataDest, data, size );
      }
      sizeToSend = header->compressedSize + sizeof( NetworkCompressionHeader );
   }

   size_t totalSent = 0;
   while( totalSent < sizeToSend )
   {
      const ssize_t sent =
          send( _clientSocket, (const char*)dataToSend + totalSent, (int)( sizeToSend - totalSent ), flags );
      if( sent < 0 )
      {
         int err = getLastNetworkError();
         if (err != HOP_NET_WOULD_BLOCK)
         {
             char msgBuffer[256];
             fprintf( stderr, "send failed: %s\n", errToStr( err, msgBuffer, 256 ) );
             break;
         }
      }
      totalSent += sent;
   }

   if( totalSent != sizeToSend ) fprintf( stderr, "Error, partial data sent\n" );

   return totalSent == sizeToSend;
}

ssize_t NetworkConnection::receiveData( void* data, size_t size ) const
{
   return recv( _clientSocket, (char*)data, size, 0 );
}

static bool sendHandshake( NetworkConnection& connection, SharedMemory& shmem )
{
   TimeStamp ts = getTimeStamp();

   NetworkHandshakeMsgInfo msg = {};
   msg.info.type               = MsgType::PROFILER_HANDSHAKE;
   msg.info.threadId           = tl_threadId;
   msg.info.threadName         = tl_threadName;
   msg.info.threadIndex        = tl_threadIndex;
   msg.info.seed               = shmem.lastResetSeed();

   msg.handshake.pid            = shmem.pid();
   msg.handshake.maxThreadCount = shmem.sharedMetaInfo()->maxThreadNb;
   msg.handshake.sharedMemSize  = HOP_SHARED_MEM_SIZE;
   msg.handshake.cpuFreqGhz     = getCpuFreqGHz();
   msg.handshake.connectionTs   = ts;
   if( g_clientName )
   {
       snprintf( msg.handshake.appName, sizeof( msg.handshake.appName ), "%s", &g_clientNameBuffer[0] );
   }
   else
   {
       snprintf( msg.handshake.appName, sizeof( msg.handshake.appName ), "hop client (%u)", shmem.pid() );
   }
   return connection.sendAllData( &msg, sizeof( msg ), true );
}

static void networkThreadLoop( NetworkConnection& connection, SharedMemory& shmem )
{
   HOP_SET_THREAD_NAME ("HOP Network Thread");

   uint8_t readBuffer[2048];
   uint8_t sendBuffer[4096*4];

   uint32_t curSendBufferSize = 0;
   while( true )
   {
      if( g_done.load() ) break;

      if( !shmem.valid() )
      {
         sleepMs( 250 );
         continue;
      }

      NetworkConnection::Status connStatus = connection.status();
      if( connStatus != NetworkConnection::Status::ALIVE )
         connection.tryConnect();

      connStatus = connection.status();
      if( connStatus == NetworkConnection::Status::ALIVE )
      {
         if( !connection.handshakeSent() )
         {
            bool sent = sendHandshake( connection, shmem );
            connection.setHandshakeSent( sent );
            shmem.setConnectedConsumer( sent );
         }

         while (connection.readReady())
         {
            ssize_t bytesToRead = connection.receiveData( readBuffer, sizeof( readBuffer ) );
            if( bytesToRead == -1 )
            {
               int err = getLastNetworkError();
               if( err == HOP_NET_CONN_RESET )
               {
                  shmem.reset();
                  connection.reset();
                  curSendBufferSize = 0;
               }
               else
                  fprintf( stderr, "HOP unknown error %d\n", err );
            }
            else
            {
               while( bytesToRead > 0 )
               {
                  ssize_t bytesRead = 0;
                  while( bytesRead < bytesToRead )
                  {
                     const ViewerMsgInfo* msg = (const ViewerMsgInfo*)( readBuffer + bytesRead );
                     if( msg->listening >= 0 ) shmem.setListeningConsumer( msg->listening );
                     if( msg->connected >= 0 )
                     {
                        shmem.setConnectedConsumer( msg->connected );
                        if( msg->connected == 0 )
                        {
                           connection.reset();
                           curSendBufferSize = 0;
                        }
                     }
                     if( msg->seed > 0 )
                     {
                        shmem.setResetSeed( msg->seed );
                        ringbuf_clear( shmem.ringbuffer() );
                     }
                     if( msg->requestHandshake ) sendHandshake( connection, shmem );
                     bytesRead += sizeof( ViewerMsgInfo );
                  }
                  bytesToRead -= bytesRead;
               }
            }
         }
      }
      else if( connStatus == NetworkConnection::Status::ERROR_ADDR_IN_USE )
      {
         int port = atoi( connection._portStr );
         /* Try the next port */
         port++;
         snprintf( connection._portStr, sizeof(connection._portStr), "%u", port);
      }

      HOP_PROF("Network Loop send data");
      size_t offset         = 0;
      const size_t size     = ringbuf_consume( shmem.ringbuffer(), &offset );
      if( size > 0 )
      {
         const bool exceed_thresh = size > sizeof( sendBuffer ) / 8;
         if( curSendBufferSize && (exceed_thresh || curSendBufferSize + size > sizeof( sendBuffer )))
         {
            const bool sent = connection.sendAllData( sendBuffer, curSendBufferSize, true );
            if( !sent )
               fprintf( stderr, "HOP - Failed to send %u bytes to remote\n", (unsigned)size );
             curSendBufferSize = 0;
         }

         if( !exceed_thresh )
         {
             memcpy( sendBuffer + curSendBufferSize, shmem.data() + offset, size );
             curSendBufferSize += size;
         }
         else
         {
             const bool sent = connection.sendAllData( shmem.data() + offset, size, true );
             if( !sent )
                fprintf( stderr, "HOP - Failed to send %u bytes to remote\n", (unsigned)size );
         }
         ringbuf_release( shmem.ringbuffer(), size );
      }
   }
}

#endif // HOP_USE_REMOTE_PROFILER

// C-style string hash inspired by Stackoverflow question
// based on the Java string hash fct. If its good enough
// for java, it should be good enough for me...
static StrPtr_t cStringHash( const char* str, size_t strLen )
{
   StrPtr_t result              = 0;
   HOP_CONSTEXPR StrPtr_t prime = 31;
   for( size_t i = 0; i < strLen; ++i )
   {
      result = str[i] + ( result * prime );
   }
   return result;
}

static uint32_t alignOn( uint32_t val, uint32_t alignment )
{
   return ( ( val + alignment - 1 ) & ~( alignment - 1 ) );
}

static uint64_t alignOn( uint64_t val, uint64_t alignment )
{
   return ( ( val + alignment - 1 ) & ~( alignment - 1 ) );
}

static void allocTraces( Traces* t, unsigned size )
{
   t->maxSize = size;
   t->starts      = (TimeStamp*)realloc( t->starts, size * sizeof( TimeStamp ) );
   t->ends        = (TimeStamp*)realloc( t->ends, size * sizeof( TimeStamp ) );
   t->depths      = (Depth_t*)realloc( t->depths, size * sizeof( Depth_t ) );
   t->fctNameIds  = (StrPtr_t*)realloc( t->fctNameIds, size * sizeof( StrPtr_t ) );
   t->fileNameIds = (StrPtr_t*)realloc( t->fileNameIds, size * sizeof( StrPtr_t ) );
   t->lineNumbers = (LineNb_t*)realloc( t->lineNumbers, size * sizeof( LineNb_t ) );
   t->zones       = (ZoneId_t*)realloc( t->zones, size * sizeof( ZoneId_t ) );
}

static void freeTraces( Traces* t )
{
   free( t->starts );
   free( t->ends );
   free( t->depths );
   free( t->fctNameIds );
   free( t->fileNameIds );
   free( t->lineNumbers );
   free( t->zones );
   memset( t, 0, sizeof( Traces ) );
}

static void addTrace(
    Traces* t,
    TimeStamp start,
    TimeStamp end,
    Depth_t depth,
    StrPtr_t fileName,
    StrPtr_t fctName,
    LineNb_t lineNb,
    ZoneId_t zone )
{
   const uint32_t curCount = t->count;
   if( curCount == t->maxSize )
   {
      allocTraces( t, t->maxSize * 2 );
   }

   t->starts[curCount]      = start;
   t->ends[curCount]        = end;
   t->depths[curCount]      = depth;
   t->fileNameIds[curCount] = fileName;
   t->fctNameIds[curCount]  = fctName;
   t->lineNumbers[curCount] = lineNb;
   t->zones[curCount]       = zone;
   ++t->count;
}

static size_t traceDataSize( const Traces* t )
{
   const size_t sliceSize = sizeof( TimeStamp ) * 2 + +sizeof( Depth_t ) + sizeof( StrPtr_t ) * 2 +
                            sizeof( LineNb_t ) + sizeof( ZoneId_t );
   return sliceSize * t->count;
}

static void copyTracesTo( const Traces* t, void* outBuffer )
{
   const uint32_t count = t->count;

   void* startsPtr         = outBuffer;
   const size_t startsSize = sizeof( t->starts[0] ) * count;
   memcpy( startsPtr, t->starts, startsSize );

   void* endsPtr         = (uint8_t*)startsPtr + startsSize;
   const size_t endsSize = sizeof( t->ends[0] ) * count;
   memcpy( endsPtr, t->ends, endsSize );

   void* fileNamesPtr         = (uint8_t*)endsPtr + endsSize;
   const size_t fileNamesSize = sizeof( t->fileNameIds[0] ) * count;
   memcpy( fileNamesPtr, t->fileNameIds, fileNamesSize );

   void* fctNamesPtr         = (uint8_t*)fileNamesPtr + fileNamesSize;
   const size_t fctNamesSize = sizeof( t->fctNameIds[0] ) * count;
   memcpy( fctNamesPtr, t->fctNameIds, fctNamesSize );

   void* lineNbPtr         = (uint8_t*)fctNamesPtr + fctNamesSize;
   const size_t lineNbSize = sizeof( t->lineNumbers[0] ) * count;
   memcpy( lineNbPtr, t->lineNumbers, lineNbSize );

   void* depthsPtr         = (uint8_t*)lineNbPtr + lineNbSize;
   const size_t depthsSize = sizeof( t->depths[0] ) * count;
   memcpy( depthsPtr, t->depths, depthsSize );

   void* zonesPtr        = (uint8_t*)depthsPtr + depthsSize;
   const size_t zoneSize = sizeof( t->zones[0] ) * count;
   memcpy( zonesPtr, t->zones, zoneSize );
}

class Client
{
  public:
   Client()
   {
      memset( &_traces, 0, sizeof( Traces ) );
      const unsigned DEFAULT_SIZE = 2 * 2;
      allocTraces( &_traces, DEFAULT_SIZE );
      _cores.reserve( 64 );
      _lockWaits.reserve( 64 );
      _unlockEvents.reserve( 64 );
      _stringPtr.reserve( 256 );
      _stringData.reserve( 256 * 32 );

      resetStringData();
   }

   ~Client() { freeTraces( &_traces ); }

   void addProfilingTrace(
       StrPtr_t fileName,
       StrPtr_t fctName,
       TimeStamp start,
       TimeStamp end,
       LineNb_t lineNb,
       ZoneId_t zone )
   {
      addTrace( &_traces, start, end, (Depth_t)tl_traceLevel, fileName, fctName, lineNb, zone );
   }

   void addCoreEvent( Core_t core, TimeStamp startTime, TimeStamp endTime )
   {
      _cores.emplace_back( CoreEvent{startTime, endTime, core} );
   }

   void addWaitLockTrace( void* mutexAddr, TimeStamp start, TimeStamp end, Depth_t depth )
   {
      _lockWaits.push_back( LockWait{mutexAddr, start, end, depth, 0 /*padding*/} );
   }

   void addUnlockEvent( void* mutexAddr, TimeStamp time )
   {
      _unlockEvents.push_back( UnlockEvent{mutexAddr, time} );
   }

   void setThreadName( StrPtr_t name )
   {
      if( !tl_threadName )
      {
         HOP_STRNCPY(
             &tl_threadNameBuffer[0],
             reinterpret_cast<const char*>( name ),
             sizeof( tl_threadNameBuffer ) - 1 );
         tl_threadNameBuffer[sizeof( tl_threadNameBuffer ) - 1] = '\0';
         tl_threadName = addDynamicStringToDb( tl_threadNameBuffer );
      }
   }

   void SetClientName( StrPtr_t name )
   {
      if( !g_clientName )
      {
         HOP_STRNCPY(
             &g_clientNameBuffer[0], reinterpret_cast<const char*>( name ), sizeof( g_clientNameBuffer ) - 1 );
         g_clientNameBuffer[sizeof( g_clientNameBuffer ) - 1] = '\0';
         g_clientName = addDynamicStringToDb( g_clientNameBuffer );
      }
   }

   StrPtr_t addDynamicStringToDb( const char* dynStr )
   {
      // Should not have null as dyn string, but just in case...
      if( dynStr == NULL ) return 0;

      const size_t strLen = strlen( dynStr );

      const StrPtr_t hash = cStringHash( dynStr, strLen );

      auto res = _stringPtr.insert( hash );
      // If the string was inserted (meaning it was not already there),
      // add it to the database, otherwise return its hash
      if( res.second )
      {
         const size_t newEntryPos = _stringData.size();
         assert( ( newEntryPos & 7 ) == 0 );  // Make sure we are 8 byte aligned
         const size_t alignedStrLen = alignOn( static_cast<uint32_t>( strLen ) + 1, 8 );

         _stringData.resize( newEntryPos + sizeof( StrPtr_t ) + alignedStrLen );
         StrPtr_t* strIdPtr = reinterpret_cast<StrPtr_t*>( &_stringData[newEntryPos] );
         *strIdPtr          = hash;
         HOP_STRNCPY( &_stringData[newEntryPos + sizeof( StrPtr_t )], dynStr, alignedStrLen );
      }

      return hash;
   }

   bool addStringToDb( StrPtr_t strId )
   {
      // Early return on NULL
      if( strId == 0 ) return false;

      auto res = _stringPtr.insert( strId );
      // If the string was inserted (meaning it was not already there),
      // add it to the database, otherwise do nothing
      if( res.second )
      {
         const size_t newEntryPos = _stringData.size();
         assert( ( newEntryPos & 7 ) == 0 );  // Make sure we are 8 byte aligned

         const size_t alignedStrLen = alignOn(
             static_cast<uint32_t>( strlen( reinterpret_cast<const char*>( strId ) ) ) + 1, 8 );

         _stringData.resize( newEntryPos + sizeof( StrPtr_t ) + alignedStrLen );
         StrPtr_t* strIdPtr = reinterpret_cast<StrPtr_t*>( &_stringData[newEntryPos] );
         *strIdPtr          = strId;
         HOP_STRNCPY(
             &_stringData[newEntryPos + sizeof( StrPtr_t )],
             reinterpret_cast<const char*>( strId ),
             alignedStrLen );
      }

      return res.second;
   }

   void resetStringData()
   {
      _stringPtr.clear();
      _stringData.clear();
      _sentStringDataSize   = 0;
      _clientResetSeed = ClientManager::sharedMemory().lastResetSeed();

      // Push back thread name
      if( tl_threadNameBuffer[0] != '\0' )
      {
         const auto hash = addDynamicStringToDb( tl_threadNameBuffer );
         HOP_UNUSED( hash );
         assert( hash == tl_threadName );
      }
      // Push back client name
      if( g_clientNameBuffer[0] != '\0' )
      {
         const auto hash = addDynamicStringToDb( g_clientNameBuffer );
         HOP_UNUSED( hash );
         assert( hash == g_clientName );
      }
   }

   void resetPendingTraces()
   {
      _traces.count = 0;
      _cores.clear();
      _lockWaits.clear();
      _unlockEvents.clear();
   }

   uint8_t* acquireSharedChunk( ringbuf_t* ringbuf, size_t size )
   {
      uint8_t* data          = NULL;
      const bool msgWayToBig = size > HOP_SHARED_MEM_SIZE;
      if( !msgWayToBig )
      {
         const size_t paddedSize = alignOn( static_cast<uint32_t>( size ), 8 );
         const ssize_t offset    = ringbuf_acquire( ringbuf, _worker, paddedSize );
         if( offset != -1 )
         {
            data = &ClientManager::sharedMemory().data()[offset];
         }
      }

      return data;
   }

   bool sendStringData( uint32_t seed )
   {
      // Add all strings to the database
      for( uint32_t i = 0; i < _traces.count; ++i )
      {
         addStringToDb( _traces.fileNameIds[i] );

         // String that were added dynamically are already in the
         // database and are flaged with the first bit of their start
         // time being 1. Therefore we only need to add the
         // non-dynamic strings. (first bit of start time being 0)
         if( ( _traces.starts[i] & 1 ) == 0 ) addStringToDb( _traces.fctNameIds[i] );
      }

      const uint32_t stringDataSize = static_cast<uint32_t>( _stringData.size() );
      assert( stringDataSize >= _sentStringDataSize );
      const uint32_t stringToSendSize = stringDataSize - _sentStringDataSize;
      const size_t msgSize            = sizeof( MsgInfo ) + stringToSendSize;

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, msgSize );

      if( !bufferPtr )
      {
         printf(
             "HOP - String to send are bigger than shared memory size. Consider"
             " increasing shared memory size \n" );
         return false;
      }

      // Fill the buffer with the string data
      {
         // The data layout is as follow:
         // =========================================================
         // msgInfo     = Profiler specific infos  - Information about the message sent
         // stringData  = String Data              - Array with all strings referenced by the traces
         MsgInfo* msgInfo = reinterpret_cast<MsgInfo*>( bufferPtr );
         char* stringData = reinterpret_cast<char*>( bufferPtr + sizeof( MsgInfo ) );

         msgInfo->type            = MsgType::PROFILER_STRING_DATA;
         msgInfo->threadId        = tl_threadId;
         msgInfo->threadName      = tl_threadName;
         msgInfo->threadIndex     = tl_threadIndex;
         msgInfo->seed       = seed;
         msgInfo->stringData.size = stringToSendSize;

         // Copy string data into its array
         const auto itFrom = _stringData.begin() + _sentStringDataSize;
         std::copy( itFrom, itFrom + stringToSendSize, stringData );
      }

      ringbuf_produce( ringbuf, _worker );

      // Update sent array size
      _sentStringDataSize = stringDataSize;

      return true;
   }

   bool sendTraces(uint32_t seed)
   {
      // Get size of profiling traces message
      const size_t profilerMsgSize = sizeof( MsgInfo ) + traceDataSize( &_traces );

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, profilerMsgSize );
      if( !bufferPtr )
      {
         printf(
             "HOP - Failed to acquire enough shared memory. Consider increasing"
             "shared memory size if you see this message more than once\n" );
         _traces.count = 0;
         return false;
      }

      // Fill the buffer with the profiling trace message
      {
         // The data layout is as follow:
         // =========================================================
         // msgInfo     = Profiler specific infos  - Information about the message sent
         // traceToSend = Traces                   - Array containing all of the traces
         MsgInfo* tracesInfo = reinterpret_cast<MsgInfo*>( bufferPtr );

         tracesInfo->type         = MsgType::PROFILER_TRACE;
         tracesInfo->threadId     = tl_threadId;
         tracesInfo->threadName   = tl_threadName;
         tracesInfo->threadIndex  = tl_threadIndex;
         tracesInfo->seed         = seed;
         tracesInfo->traces.count = _traces.count;

         // Copy trace information into buffer to send
         void* outBuffer = (void*)( bufferPtr + sizeof( MsgInfo ) );
         copyTracesTo( &_traces, outBuffer );
      }

      ringbuf_produce( ringbuf, _worker );

      _traces.count = 0;

      return true;
   }

   bool sendCores( uint32_t seed )
   {
      if( _cores.empty() ) return false;

      const size_t coreMsgSize = sizeof( MsgInfo ) + _cores.size() * sizeof( CoreEvent );

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, coreMsgSize );
      if( !bufferPtr )
      {
         printf(
             "HOP - Failed to acquire enough shared memory. Consider increasing shared memory "
             "size\n" );
         _cores.clear();
         return false;
      }

      // Fill the buffer with the core event message
      {
         MsgInfo* coreInfo          = reinterpret_cast<MsgInfo*>( bufferPtr );
         coreInfo->type             = MsgType::PROFILER_CORE_EVENT;
         coreInfo->threadId         = tl_threadId;
         coreInfo->threadName       = tl_threadName;
         coreInfo->threadIndex      = tl_threadIndex;
         coreInfo->seed             = seed;
         coreInfo->coreEvents.count = static_cast<uint32_t>( _cores.size() );
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _cores.data(), _cores.size() * sizeof( CoreEvent ) );
      }

      ringbuf_produce( ringbuf, _worker );

      const auto lastEntry = _cores.back();
      _cores.clear();
      _cores.emplace_back( lastEntry );

      return true;
   }

   bool sendLockWaits( uint32_t seed )
   {
      if( _lockWaits.empty() ) return false;

      const size_t lockMsgSize = sizeof( MsgInfo ) + _lockWaits.size() * sizeof( LockWait );

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, lockMsgSize );
      if( !bufferPtr )
      {
         printf(
             "HOP - Failed to acquire enough shared memory. Consider increasing shared memory "
             "size\n" );
         _lockWaits.clear();
         return false;
      }

      // Fill the buffer with the lock message
      {
         MsgInfo* lwInfo         = reinterpret_cast<MsgInfo*>( bufferPtr );
         lwInfo->type            = MsgType::PROFILER_WAIT_LOCK;
         lwInfo->threadId        = tl_threadId;
         lwInfo->threadName      = tl_threadName;
         lwInfo->threadIndex     = tl_threadIndex;
         lwInfo->seed       = seed;
         lwInfo->lockwaits.count = static_cast<uint32_t>( _lockWaits.size() );
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      }

      ringbuf_produce( ringbuf, _worker );

      _lockWaits.clear();

      return true;
   }

   bool sendUnlockEvents( uint32_t seed )
   {
      if( _unlockEvents.empty() ) return false;

      const size_t unlocksMsgSize =
          sizeof( MsgInfo ) + _unlockEvents.size() * sizeof( UnlockEvent );

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, unlocksMsgSize );
      if( !bufferPtr )
      {
         printf(
             "HOP - Failed to acquire enough shared memory. Consider increasing shared memory "
             "size\n" );
         _unlockEvents.clear();
         return false;
      }

      // Fill the buffer with the lock message
      {
         MsgInfo* uInfo            = reinterpret_cast<MsgInfo*>( bufferPtr );
         uInfo->type               = MsgType::PROFILER_UNLOCK_EVENT;
         uInfo->threadId           = tl_threadId;
         uInfo->threadName         = tl_threadName;
         uInfo->threadIndex        = tl_threadIndex;
         uInfo->seed               = seed;
         uInfo->unlockEvents.count = static_cast<uint32_t>( _unlockEvents.size() );
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _unlockEvents.data(), _unlockEvents.size() * sizeof( UnlockEvent ) );
      }

      ringbuf_produce( ringbuf, _worker );

      _unlockEvents.clear();

      return true;
   }

   bool sendHeartbeat( TimeStamp timeStamp, uint32_t seed )
   {
      ClientManager::SetLastHeartbeatTimestamp( timeStamp );

      const size_t heartbeatSize = sizeof( MsgInfo );

      ringbuf_t* ringbuf = ClientManager::sharedMemory().ringbuffer();
      uint8_t* bufferPtr = acquireSharedChunk( ringbuf, heartbeatSize );
      if( !bufferPtr )
      {
         printf(
             "HOP - Failed to acquire enough shared memory. Consider increasing shared memory "
             "size\n" );
         return false;
      }

      // Fill the buffer with the lock message
      {
         MsgInfo* hbInfo     = reinterpret_cast<MsgInfo*>( bufferPtr );
         hbInfo->type        = MsgType::PROFILER_HEARTBEAT;
         hbInfo->threadId    = tl_threadId;
         hbInfo->threadName  = tl_threadName;
         hbInfo->threadIndex = tl_threadIndex;
         hbInfo->seed        = seed;
         bufferPtr += sizeof( MsgInfo );
      }

      ringbuf_produce( ringbuf, _worker );

      return true;
   }

   void flushToConsumer()
   {
      // If we have a consumer, send life signal
      const TimeStamp timeStamp = getTimeStamp();
      const uint32_t seed  = ClientManager::sharedMemory().lastResetSeed();
      if( ClientManager::HasConnectedConsumer() && ClientManager::ShouldSendHeartbeat( timeStamp ) )
      {
         sendHeartbeat( timeStamp, seed );
      }

      // If no one is there to listen, no need to send any data
      if( ClientManager::HasListeningConsumer() )
      {
         // If the shared memory reset timestamp more recent than our local one
         // it means we need to clear our string table. Otherwise it means we
         // already took care of it. Since some traces might depend on strings
         // that were added dynamically (ie before clearing the db), we cannot
         // consider them and need to return here.
         if( _clientResetSeed < seed)
         {
            resetStringData();
            resetPendingTraces();
            return;
         }

         sendStringData( seed );  // Always send string data first
         sendTraces( seed );
         sendLockWaits( seed );
         sendUnlockEvents( seed );
         sendCores( seed );
      }
      else
      {
         resetPendingTraces();
      }
   }

   Traces _traces;
   std::vector<CoreEvent> _cores;
   std::vector<LockWait> _lockWaits;
   std::vector<UnlockEvent> _unlockEvents;
   std::unordered_set<StrPtr_t> _stringPtr;
   std::vector<char> _stringData;
   uint32_t _clientResetSeed{0};
   ringbuf_worker_t* _worker{NULL};
   uint32_t _sentStringDataSize{0};  // The size of the string array on viewer side
};

Client* ClientManager::Get()
{
   static bool initialized;
   HOP_NO_DESTROY thread_local std::unique_ptr<Client> threadClient;

   if( unlikely( g_done.load() ) ) return nullptr;
   if( likely( threadClient.get() ) ) return threadClient.get();

   // If we have not yet created our shared memory segment, do it here
   if( !initialized )
   {
      // Make sure only the first thread to ever get here can create the shared memory
      HOP_NO_DESTROY static std::mutex g_creationMutex;
      std::lock_guard<std::mutex> g( g_creationMutex );

      if( !initialized )
      {
         ConnectionState state =
             ClientManager::sharedMemory().create( HOP_GET_PID(), HOP_SHARED_MEM_SIZE, false );
         if( state != CONNECTED )
         {
            const char* reason = "";
            if( state == PERMISSION_DENIED ) reason = " : Permission Denied";

            fprintf(stderr, "HOP - Could not create shared memory%s. HOP will not be able to run\n", reason );
            return NULL;
         }

   #if HOP_USE_REMOTE_PROFILER
         if( state == CONNECTED )
         {
            ClientManager::networkConnection().start( ClientManager::sharedMemory() );
         }
   #endif
        initialized = true;
      }
   }

   // Atomically get the next thread id from the static atomic count
   static std::atomic<uint32_t> threadCount{0};
   tl_threadIndex = threadCount.fetch_add( 1 );
   tl_threadId    = HOP_GET_THREAD_ID();

   if( tl_threadIndex > HOP_MAX_THREAD_NB )
   {
      printf( "Maximum number of threads reached. No trace will be available for this thread\n" );
      return nullptr;
   }

   // Register producer in the ringbuffer
   auto ringBuffer = ClientManager::sharedMemory().ringbuffer();
   if( ringBuffer )
   {
      threadClient.reset( new Client() );
      threadClient->_worker = ringbuf_register( ringBuffer, tl_threadIndex );
      if( threadClient->_worker == NULL )
      {
         assert( false && "ringbuf_register" );
      }
   }

   return threadClient.get();
}

ZoneId_t ClientManager::StartProfile()
{
   ++tl_traceLevel;
   return tl_zoneId;
}

StrPtr_t ClientManager::StartProfileDynString( const char* str, ZoneId_t* zone )
{
   ++tl_traceLevel;
   Client* client = ClientManager::Get();

   if( unlikely( !client ) ) return 0;

   *zone = tl_zoneId;
   return client->addDynamicStringToDb( str );
}

void ClientManager::EndProfile(
    StrPtr_t fileName,
    StrPtr_t fctName,
    TimeStamp start,
    TimeStamp end,
    LineNb_t lineNb,
    ZoneId_t zone,
    Core_t core )
{
   const int remainingPushedTraces = --tl_traceLevel;
   Client* client                  = ClientManager::Get();

   if( unlikely( !client ) ) return;

   if( end - start > 50 )  // Minimum trace time is 50 ns
   {
      client->addProfilingTrace( fileName, fctName, start, end, lineNb, zone );
      client->addCoreEvent( core, start, end );
   }
   if( remainingPushedTraces <= 0 )
   {
      client->flushToConsumer();
   }
}

void ClientManager::EndLockWait( void* mutexAddr, TimeStamp start, TimeStamp end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code
   if( tl_traceLevel > 0 && end - start >= HOP_MIN_LOCK_CYCLES )
   {
      auto client = ClientManager::Get();
      if( unlikely( !client ) ) return;

      client->addWaitLockTrace(
          mutexAddr, start, end, static_cast<unsigned short>( tl_traceLevel ) );
   }
}

void ClientManager::UnlockEvent( void* mutexAddr, TimeStamp time )
{
   if( tl_traceLevel > 0 )
   {
      auto client = ClientManager::Get();
      if( unlikely( !client ) ) return;

      client->addUnlockEvent( mutexAddr, time );
   }
}

void ClientManager::SetThreadName( const char* name ) HOP_NOEXCEPT
{
   auto client = ClientManager::Get();
   if( unlikely( !client ) ) return;

   client->setThreadName( reinterpret_cast<StrPtr_t>( name ) );
}

void ClientManager::SetClientName( const char* name ) HOP_NOEXCEPT
{
   auto client = ClientManager::Get();
   if( unlikely( !client ) ) return;

   client->SetClientName( reinterpret_cast<StrPtr_t>( name ) );
}

ZoneId_t ClientManager::PushNewZone( ZoneId_t newZone )
{
   ZoneId_t prevZone = tl_zoneId;
   tl_zoneId         = newZone;
   return prevZone;
}

bool ClientManager::HasConnectedConsumer() HOP_NOEXCEPT
{
   return ClientManager::sharedMemory().valid() &&
          ClientManager::sharedMemory().hasConnectedConsumer();
}

bool ClientManager::HasListeningConsumer() HOP_NOEXCEPT
{
   return ClientManager::sharedMemory().valid() &&
          ClientManager::sharedMemory().hasListeningConsumer();
}

bool ClientManager::ShouldSendHeartbeat( TimeStamp t ) HOP_NOEXCEPT
{
   return ClientManager::sharedMemory().valid() &&
          ClientManager::sharedMemory().shouldSendHeartbeat( t );
}

void ClientManager::SetLastHeartbeatTimestamp( TimeStamp t ) HOP_NOEXCEPT
{
   ClientManager::sharedMemory().setLastHeartbeatTimestamp( t );
}

SharedMemory& ClientManager::sharedMemory() HOP_NOEXCEPT
{
   HOP_NO_DESTROY static SharedMemory _sharedMemory;
   return _sharedMemory;
}

#if HOP_USE_REMOTE_PROFILER
NetworkConnection& ClientManager::networkConnection() HOP_NOEXCEPT
{
   HOP_NO_DESTROY static NetworkConnection _networkConnection;
   return _networkConnection;
}
#endif

void ClientManager::Shutdown() HOP_NOEXCEPT
{
   g_done.store( true );
#if HOP_USE_REMOTE_PROFILER
   networkConnection().stop();
#endif
   sharedMemory().destroy();
}

}  // end of namespace hop

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <algorithm>
#include <limits.h>

/*
 * Exponential back-off for the spinning paths.
 */
#define SPINLOCK_BACKOFF_MIN 4
#define SPINLOCK_BACKOFF_MAX 128
#if defined( __x86_64__ ) || defined( __i386__ )
#define SPINLOCK_BACKOFF_HOOK __asm volatile( "pause" ::: "memory" )
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define SPINLOCK_BACKOFF( count )                                    \
   do                                                                \
   {                                                                 \
      for( int __i = ( count ); __i != 0; __i-- )                    \
      {                                                              \
         SPINLOCK_BACKOFF_HOOK;                                      \
      }                                                              \
      if( ( count ) < SPINLOCK_BACKOFF_MAX ) ( count ) += ( count ); \
   } while( /* CONSTCOND */ 0 );

#define RBUF_OFF_MASK ( 0x00000000ffffffffUL )
#define WRAP_LOCK_BIT ( 0x8000000000000000UL )
#define RBUF_OFF_MAX ( UINT64_MAX & ~WRAP_LOCK_BIT )

#define WRAP_COUNTER ( 0x7fffffff00000000UL )
#define WRAP_INCR( x ) ( ( ( x ) + 0x100000000UL ) & WRAP_COUNTER )

typedef uint64_t ringbuf_off_t;

struct ringbuf_worker
{
   volatile ringbuf_off_t seen_off;
   int registered;
};

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4200 )  // Warning C4200 nonstandard extension used: zero-sized array in
                                   // struct/union
#endif
struct ringbuf
{
   /* Ring buffer space. */
   size_t space;

   /*
    * The NEXT hand is atomically updated by the producer.
    * WRAP_LOCK_BIT is set in case of wrap-around; in such case,
    * the producer can update the 'end' offset.
    */
   std::atomic<ringbuf_off_t> next;
   ringbuf_off_t end;

   /* The following are updated by the consumer. */
   ringbuf_off_t written;
   unsigned nworkers;
   ringbuf_worker_t workers[];
};
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

/*
 * ringbuf_setup: initialise a new ring buffer of a given length.
 */
int ringbuf_setup( ringbuf_t* rbuf, unsigned nworkers, size_t length )
{
   if( length >= RBUF_OFF_MASK )
   {
      return -1;
   }
   rbuf->next.store(0);
   rbuf->written  = 0;
   rbuf->space    = length;
   rbuf->end      = RBUF_OFF_MAX;
   rbuf->nworkers = nworkers;
   return 0;
}

/*
 * ringbuf_get_sizes: return the sizes of the ringbuf_t and ringbuf_worker_t.
 */
void ringbuf_get_sizes( const unsigned nworkers, size_t* ringbuf_size, size_t* ringbuf_worker_size )
{
   if( ringbuf_size )
   {
      *ringbuf_size = offsetof( ringbuf_t, workers ) + sizeof( ringbuf_worker_t ) * nworkers;
   }
   if( ringbuf_worker_size )
   {
      *ringbuf_worker_size = sizeof( ringbuf_worker_t );
   }
}

/*
 * ringbuf_register: register the worker (thread/process) as a producer
 * and pass the pointer to its local store.
 */
ringbuf_worker_t* ringbuf_register( ringbuf_t* rbuf, unsigned i )
{
   ringbuf_worker_t* w = &rbuf->workers[i];

   w->seen_off = RBUF_OFF_MAX;
   std::atomic_thread_fence( std::memory_order_release );
   w->registered = true;
   return w;
}

void ringbuf_unregister( ringbuf_t*, ringbuf_worker_t* w ) { w->registered = false; }

/*
 * stable_nextoff: capture and return a stable value of the 'next' offset.
 */
static inline ringbuf_off_t stable_nextoff( ringbuf_t* rbuf )
{
   unsigned count = SPINLOCK_BACKOFF_MIN;
   ringbuf_off_t next;

   while( ( next = rbuf->next ) & WRAP_LOCK_BIT )
   {
      SPINLOCK_BACKOFF( count );
   }
   std::atomic_thread_fence( std::memory_order_acquire );
   assert( ( next & RBUF_OFF_MASK ) < rbuf->space );
   return next;
}

/*
 * ringbuf_acquire: request a space of a given length in the ring buffer.
 *
 * => On success: returns the offset at which the space is available.
 * => On failure: returns -1.
 */
ssize_t ringbuf_acquire( ringbuf_t* rbuf, ringbuf_worker_t* w, size_t len )
{
   ringbuf_off_t seen, next, target;

   assert( len > 0 && len <= rbuf->space );
   assert( w->seen_off == RBUF_OFF_MAX );

   do
   {
      ringbuf_off_t written;

      /*
       * Get the stable 'next' offset.  Save the observed 'next'
       * value (i.e. the 'seen' offset), but mark the value as
       * unstable (set WRAP_LOCK_BIT).
       *
       * Note: CAS will issue a std::memory_order_release for us and
       * thus ensures that it reaches global visibility together
       * with new 'next'.
       */
      seen = stable_nextoff( rbuf );
      next = seen & RBUF_OFF_MASK;
      assert( next < rbuf->space );
      w->seen_off = next | WRAP_LOCK_BIT;

      /*
       * Compute the target offset.  Key invariant: we cannot
       * go beyond the WRITTEN offset or catch up with it.
       */
      target  = next + len;
      written = rbuf->written;
      if( unlikely( next < written && target >= written ) )
      {
         /* The producer must wait. */
         w->seen_off = RBUF_OFF_MAX;
         return -1;
      }

      if( unlikely( target >= rbuf->space ) )
      {
         const bool exceed = target > rbuf->space;

         /*
          * Wrap-around and start from the beginning.
          *
          * If we would exceed the buffer, then attempt to
          * acquire the WRAP_LOCK_BIT and use the space in
          * the beginning.  If we used all space exactly to
          * the end, then reset to 0.
          *
          * Check the invariant again.
          */
         target = exceed ? ( WRAP_LOCK_BIT | len ) : 0;
         if( ( target & RBUF_OFF_MASK ) >= written )
         {
            w->seen_off = RBUF_OFF_MAX;
            return -1;
         }
         /* Increment the wrap-around counter. */
         target |= WRAP_INCR( seen & WRAP_COUNTER );
      }
      else
      {
         /* Preserve the wrap-around counter. */
         target |= seen & WRAP_COUNTER;
      }
   } while( !std::atomic_compare_exchange_weak( &rbuf->next, &seen, target ) );

   /*
    * Acquired the range.  Clear WRAP_LOCK_BIT in the 'seen' value
    * thus indicating that it is stable now.
    */
   w->seen_off &= ~WRAP_LOCK_BIT;

   /*
    * If we set the WRAP_LOCK_BIT in the 'next' (because we exceed
    * the remaining space and need to wrap-around), then save the
    * 'end' offset and release the lock.
    */
   if( unlikely( target & WRAP_LOCK_BIT ) )
   {
      /* Cannot wrap-around again if consumer did not catch-up. */
      assert( rbuf->written <= next );
      assert( rbuf->end == RBUF_OFF_MAX );
      rbuf->end = next;
      next      = 0;

      /*
       * Unlock: ensure the 'end' offset reaches global
       * visibility before the lock is released.
       */
      std::atomic_thread_fence( std::memory_order_release );
      rbuf->next = ( target & ~WRAP_LOCK_BIT );
   }
   assert( ( target & RBUF_OFF_MASK ) <= rbuf->space );
   return static_cast<ssize_t>( next );
}

/*
 * ringbuf_produce: indicate the acquired range in the buffer is produced
 * and is ready to be consumed.
 */
void ringbuf_produce( ringbuf_t*, ringbuf_worker_t* w )
{
   assert( w->registered );
   assert( w->seen_off != RBUF_OFF_MAX );
   std::atomic_thread_fence( std::memory_order_release );
   w->seen_off = RBUF_OFF_MAX;
}

/*
 * ringbuf_consume: get a contiguous range which is ready to be consumed.
 */
size_t ringbuf_consume( ringbuf_t* rbuf, size_t* offset )
{
   ringbuf_off_t written = rbuf->written, next, ready;
   size_t towrite;
retry:
   /*
    * Get the stable 'next' offset.  Note: stable_nextoff() issued
    * a load memory barrier.  The area between the 'written' offset
    * and the 'next' offset will be the *preliminary* target buffer
    * area to be consumed.
    */
   next = stable_nextoff( rbuf ) & RBUF_OFF_MASK;
   if( written == next )
   {
      /* If producers did not advance, then nothing to do. */
      return 0;
   }

   /*
    * Observe the 'ready' offset of each producer.
    *
    * At this point, some producer might have already triggered the
    * wrap-around and some (or all) seen 'ready' values might be in
    * the range between 0 and 'written'.  We have to skip them.
    */
   ready = RBUF_OFF_MAX;

   for( unsigned i = 0; i < rbuf->nworkers; i++ )
   {
      ringbuf_worker_t* w = &rbuf->workers[i];
      unsigned count      = SPINLOCK_BACKOFF_MIN;
      ringbuf_off_t seen_off;

      /* Skip if the worker has not registered. */
      if( !w->registered )
      {
         continue;
      }

      /*
       * Get a stable 'seen' value.  This is necessary since we
       * want to discard the stale 'seen' values.
       */
      while( ( seen_off = w->seen_off ) & WRAP_LOCK_BIT )
      {
         SPINLOCK_BACKOFF( count );
      }

      /*
       * Ignore the offsets after the possible wrap-around.
       * We are interested in the smallest seen offset that is
       * not behind the 'written' offset.
       */
      if( seen_off >= written )
      {
         ready = HOP_MIN( seen_off, ready );
      }
      assert( ready >= written );
   }

   /*
    * Finally, we need to determine whether wrap-around occurred
    * and deduct the safe 'ready' offset.
    */
   if( next < written )
   {
      const ringbuf_off_t end = HOP_MIN( static_cast<ringbuf_off_t>( rbuf->space ), rbuf->end );

      /*
       * Wrap-around case.  Check for the cut off first.
       *
       * Reset the 'written' offset if it reached the end of
       * the buffer or the 'end' offset (if set by a producer).
       * However, we must check that the producer is actually
       * done (the observed 'ready' offsets are clear).
       */
      if( ready == RBUF_OFF_MAX && written == end )
      {
         /*
          * Clear the 'end' offset if was set.
          */
         if( rbuf->end != RBUF_OFF_MAX )
         {
            rbuf->end = RBUF_OFF_MAX;
            std::atomic_thread_fence( std::memory_order_release );
         }
         /* Wrap-around the consumer and start from zero. */
         rbuf->written = written = 0;
         goto retry;
      }

      /*
       * We cannot wrap-around yet; there is data to consume at
       * the end.  The ready range is smallest of the observed
       * 'ready' or the 'end' offset.  If neither is set, then
       * the actual end of the buffer.
       */
      assert( ready > next );
      ready = HOP_MIN( ready, end );
      assert( ready >= written );
   }
   else
   {
      /*
       * Regular case.  Up to the observed 'ready' (if set)
       * or the 'next' offset.
       */
      ready = HOP_MIN( ready, next );
   }
   towrite = ready - written;
   *offset = written;

   assert( ready >= written );
   assert( towrite <= rbuf->space );
   return towrite;
}

/*
 * ringbuf_release: indicate that the consumed range can now be released.
 */
void ringbuf_release( ringbuf_t* rbuf, size_t nbytes )
{
   const size_t nwritten = rbuf->written + nbytes;

   assert( rbuf->written <= rbuf->space );
   assert( rbuf->written <= rbuf->end );
   assert( nwritten <= rbuf->space );

   rbuf->written = ( nwritten == rbuf->space ) ? 0 : nwritten;
}

void ringbuf_clear( ringbuf_t* rbuf )
{
   size_t offset = 0;
   while ( size_t bytesToRead = ringbuf_consume( rbuf, &offset ) )
     ringbuf_release( rbuf, bytesToRead );
}

#if HOP_USE_REMOTE_PROFILER

/*
 * LZJB Compression implementation
 */

#define LZJB_MATCH_BITS   6
#define LZJB_MATCH_MIN    3
#define LZJB_MATCH_MAX    ((1 << LZJB_MATCH_BITS) + (LZJB_MATCH_MIN - 1))
#define LZJB_OFFSET_MASK  ((1 << (16 - LZJB_MATCH_BITS)) - 1)
#define LZJB_LEMPEL_SIZE  1024

#ifndef NBBY
#define NBBY CHAR_BIT
#endif

size_t
lzjb_compress(const void *s_start, void *d_start, size_t s_len, size_t d_len)
{
   const uint8_t *src = (const uint8_t*)s_start;
   const uint8_t *cpy = NULL;
   uint8_t *dst = (uint8_t*)d_start;
   uint8_t *copymap = NULL;
   int copymask = 1 << (NBBY - 1);
   int mlen, offset, hash;
   uint16_t *hp;
   uint16_t lempel[LZJB_LEMPEL_SIZE] = { 0 };

   while (src < (uint8_t *)s_start + s_len) {
      if ((copymask <<= 1) == (1 << NBBY)) {
         if (dst >= (uint8_t *)d_start + d_len - 1 - 2 * NBBY)
            return (s_len);
         copymask = 1;
         copymap = dst;
         *dst++ = 0;
      }
      if (src > (uint8_t *)s_start + s_len - LZJB_MATCH_MAX) {
         *dst++ = *src++;
         continue;
      }
      hash = (src[0] << 16) + (src[1] << 8) + src[2];
      hash += hash >> 9;
      hash += hash >> 5;
      hp = &lempel[hash & (LZJB_LEMPEL_SIZE - 1)];
      offset = (intptr_t)(src - *hp) & LZJB_OFFSET_MASK;
      *hp = (uint16_t)(uintptr_t)src;
      cpy = src - offset;
      if (cpy >= (uint8_t *)s_start && cpy != src &&
          src[0] == cpy[0] && src[1] == cpy[1] && src[2] == cpy[2]) {
         *copymap |= copymask;
         for (mlen = LZJB_MATCH_MIN; mlen < LZJB_MATCH_MAX; mlen++)
            if (src[mlen] != cpy[mlen])
               break;
         *dst++ = (uint8_t)(((mlen - LZJB_MATCH_MIN) << (NBBY - LZJB_MATCH_BITS)) | (offset >> NBBY));
         *dst++ = (uint8_t)offset;
         src += mlen;
      } else {
         *dst++ = *src++;
      }
   }
   return (dst - (uint8_t *)d_start);
}

int
lzjb_decompress(const void *s_start, void *d_start, size_t s_len, size_t d_len)
{
   (void)s_len;
   const uint8_t *src = (const uint8_t*)s_start;
   uint8_t *dst = (uint8_t *)d_start;
   uint8_t *d_end = (uint8_t *)d_start + d_len;
   uint8_t *cpy = NULL, copymap = 0;
   int copymask = 1 << (NBBY - 1);

   while (dst < d_end) {
      if ((copymask <<= 1) == (1 << NBBY)) {
         copymask = 1;
         copymap = *src++;
      }
      if (copymap & copymask) {
         int mlen = (src[0] >> (NBBY - LZJB_MATCH_BITS)) + LZJB_MATCH_MIN;
         int offset = ((src[0] << NBBY) | src[1]) & LZJB_OFFSET_MASK;
         src += 2;
         if ((cpy = dst - offset) < (uint8_t *)d_start)
            return (-1);
         while (--mlen >= 0 && dst < d_end)
            *dst++ = *cpy++;
      } else {
         *dst++ = *src++;
      }
   }
   return (0);
}

#endif  // HOP_USE_REMOTE_PROFILER

#endif  // end HOP_IMPLEMENTATION

#endif  // !defined(HOP_ENABLED)

#endif  // HOP_H_
