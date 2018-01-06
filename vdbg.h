#ifndef VDBG_H_
#define VDBG_H_

// You can disable completly vdbg by setting this variable
// to false
#if !defined( VDBG_ENABLED )

// Stubbing all profiling macros so they are disabled
// when VDBG_ENABLED is false
#define VDBG_PROF_FUNC()
#define VDBG_PROF_MEMBER_FUNC()
#define VDBG_PROF_FUNC_WITH_GROUP( x )
#define VDBG_PROF_MEMBER_FUNC_WITH_GROUP( x )

#else  // We do want to profile


///////////////////////////////////////////////////////////////
/////    THESE ARE THE MACROS YOU SHOULD USE        ///////////
///////////////////////////////////////////////////////////////

#define VDBG_SHARED_MEM_SIZE 32000000

// Create a new profiling trace for a free function
#define VDBG_PROF_FUNC() VDBG_PROF_GUARD_VAR( __LINE__, ( __func__, NULL, 0 ) )
// Create a new profiling trace for a member function
#define VDBG_PROF_MEMBER_FUNC() \
   VDBG_PROF_GUARD_VAR( __LINE__, ( __func__, typeid( this ).name(), 0 ) )
// Create a new profiling trace for a free function that falls under category x
#define VDBG_PROF_FUNC_WITH_GROUP( x ) VDBG_PROF_GUARD_VAR(__LINE__,(( __func__, NULL, x ))
// Create a new profiling trace for a member function that falls under category x
#define VDBG_PROF_MEMBER_FUNC_WITH_GROUP( x ) \
   VDBG_PROF_GUARD_VAR(__LINE__,(( __func__, typeid( this ).name(), x ))

///////////////////////////////////////////////////////////////
/////     EVERYTHING AFTER THIS IS IMPL DETAILS        ////////
///////////////////////////////////////////////////////////////

#include <stdint.h>
#include <chrono>
#include <thread>

// ------ platform.h ------------
// This is most things that are potentially non-portable.
#define VDBG_CONSTEXPR constexpr
#define VDBG_NOEXCEPT noexcept
#define VDBG_STATIC_ASSERT static_assert
#define VDBG_GET_THREAD_ID() (size_t)pthread_self()
#define VDBG_SERVER_PATH "/tmp/my_server"
extern char* __progname;
inline const char* getProgName() VDBG_NOEXCEPT
{
   return __progname;
}
// -----------------------------

// ------ message.h ------------
namespace vdbg
{

using Clock = std::chrono::high_resolution_clock;
using Precision = std::chrono::nanoseconds;
inline decltype( std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count() ) getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

namespace details
{

VDBG_CONSTEXPR uint32_t EXPECTED_MSG_HEADER_SIZE = 8;
struct MsgHeader
{
   // Thread id from which the msg was sent
   uint32_t threadId;
   // Number of message in this communication
   uint32_t msgCount;
};
VDBG_STATIC_ASSERT(
    sizeof( MsgHeader ) == EXPECTED_MSG_HEADER_SIZE,
    "MsgHeader layout has changed unexpectedly" );

enum class MsgInfoType : uint32_t
{
   PROFILER_TRACE,
   PROFILER_WAIT_LOCK,
   INVALID_MESSAGE,
};

struct TracesMsgInfo
{
   uint32_t stringDataSize;
   uint32_t traceCount;
};

struct LockWaitsMsgInfo
{
   uint32_t count;
};

VDBG_CONSTEXPR uint32_t EXPECTED_MSG_INFO_SIZE = 16;
struct MsgInfo
{
   MsgInfoType type;
   union {
      TracesMsgInfo traces;
      LockWaitsMsgInfo lockwaits;
   };
   uint32_t padding;
};
VDBG_STATIC_ASSERT( sizeof(MsgInfo) == EXPECTED_MSG_INFO_SIZE, "MsgInfo layout has changed unexpectedly" );


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

VDBG_CONSTEXPR uint32_t EXPECTED_LOCK_WAIT_SIZE = 32;
struct LockWait
{
   void* mutexAddress;
   TimeStamp start, end;
   uint32_t padding;
};
VDBG_STATIC_ASSERT( sizeof(LockWait) == EXPECTED_LOCK_WAIT_SIZE, "Lock wait layout has changed unexpectedly" );

// ------ end of message.h ------------

// -------- vdbg_client.h -------------
static constexpr int MAX_THREAD_NB = 64;
class ClientProfiler
{
  public:
   class Impl;
   static Impl* Get( size_t threadId, bool createIfMissing = true );
   static void StartProfile( Impl* );
   static void EndProfile(
       Impl*,
       const char* name,
       const char* classStr,
       TimeStamp start,
       TimeStamp end,
       uint8_t group );
   static void EndLockWait(
      Impl*,
      void* mutexAddr,
      TimeStamp start,
      TimeStamp end );

  private:
   static void* sharedMemory;
   static size_t threadsId[MAX_THREAD_NB];
   static ClientProfiler::Impl* clientProfilers[MAX_THREAD_NB];
};

class ProfGuard
{
  public:
   ProfGuard( const char* name, const char* classStr, uint8_t groupId ) VDBG_NOEXCEPT
       : start( getTimeStamp() ),
         className( classStr ),
         fctName( name ),
         impl( ClientProfiler::Get( VDBG_GET_THREAD_ID() ) ),
         group( groupId )
   {
      ClientProfiler::StartProfile( impl );
   }
   ~ProfGuard()
   {
      end = getTimeStamp();
      ClientProfiler::EndProfile( impl, fctName, className, start, end, group );
   }

  private:
   TimeStamp start, end;
   const char *className, *fctName;
   ClientProfiler::Impl* impl;
   uint8_t group;
};

struct LockWaitGuard
{
   LockWaitGuard( void* mutAddr )
       : start( getTimeStamp() ),
         mutexAddr( mutAddr ),
         impl( ClientProfiler::Get( VDBG_GET_THREAD_ID() ) )
   {
   }
   ~LockWaitGuard()
   {
      end = getTimeStamp();
      ClientProfiler::EndLockWait( impl, mutexAddr, start, end );
   }

   TimeStamp start, end;
   void* mutexAddr;
   ClientProfiler::Impl* impl;
};

#define VDBG_COMBINE( X, Y ) X##Y
#define VDBG_PROF_GUARD_VAR( LINE, ARGS ) \
   vdbg::details::ProfGuard VDBG_COMBINE( vdbgProfGuard, LINE ) ARGS

// -------- end of vdbg_client.h -------------

// ------ SharedMemory.h ------------
class SharedMemory
{
public:
   bool create( const char* path, size_t size );
   ~SharedMemory();
   bool ok() const VDBG_NOEXCEPT;
private:
   void* _data{NULL};
   size_t _size{0};
   const char* _path{NULL};
   int _sharedMemFd{-1};
   int _semaphoreId{-1};
};

}  // namespace details
}  // namespace vdbg

// End of vdbg declarations

// ------------ ringbuf.h by Mindaugas Rasiukevicius ------------

typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;

int      ringbuf_setup(ringbuf_t *, unsigned, size_t);
void     ringbuf_get_sizes(unsigned, size_t *, size_t *);

ringbuf_worker_t *ringbuf_register(ringbuf_t *, unsigned);
void     ringbuf_unregister(ringbuf_t *, ringbuf_worker_t *);

ssize_t     ringbuf_acquire(ringbuf_t *, ringbuf_worker_t *, size_t);
void     ringbuf_produce(ringbuf_t *, ringbuf_worker_t *);
size_t      ringbuf_consume(ringbuf_t *, size_t *);
void     ringbuf_release(ringbuf_t *, size_t);

// ------------ End of ringbuf.h   ------------

#ifdef VDBG_IMPLEMENTATION

// C sockets include
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// C++ standard includes
#include <algorithm>
#include <cassert>
#include <vector>

namespace vdbg
{
namespace details
{

// ------ SharedMemory.cpp ------------

bool SharedMemory::create( const char* path, size_t size )
{
   // TODO handle signals 
   //signal( SIGINT, sig_callback_handler );
   _path = path;
   _sharedMemFd = shm_open( path, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG );
   if ( _sharedMemFd < 0 )
   {
      perror( "Cannot create shared memory" );
   }

   ftruncate( _sharedMemFd, size );

   /**
    * Semaphore open
    */
   _semaphoreId = sem_open( "/mysem", O_CREAT, S_IRUSR | S_IWUSR, 1 );

   _data = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, _sharedMemFd, 0 );
   if ( _data == NULL )
   {
      perror( "Could not map shared memory" );
   }
}

bool ok() const VDBG_NOEXCEPT
{
   return _data != NULL;
}

SharedMemory::~SharedMemory()
{
   if( _data )
   {
      if ( shm_unlink( _path ) != 0 )
      {
         perror( "Could not unlink shared memory" );
      }
   }
}

// ------ end of SharedMemory.cpp ------------


// ------ cdbg_client.cpp------------

SharedMemory ClientProfiler::sharedMemory;
size_t ClientProfiler::threadsId[MAX_THREAD_NB] = {0};
ClientProfiler::Impl* ClientProfiler::clientProfilers[MAX_THREAD_NB] = {0};

class ClientProfiler::Impl
{
   struct ShallowTrace
   {
      const char *className, *fctName;
      TimeStamp start, end;
      uint8_t group;
   };

  public:
   Impl(size_t id) : _threadId( id )
   {
      _shallowTraces.reserve( 64 );
      _lockWaits.reserve( 64 );
      _nameArrayId.reserve( 64 );
      _nameArrayData.reserve( 64 * 32 );

      // Push back first name as empty string
      _nameArrayData.push_back('\0');
      _nameArrayId.push_back(NULL);
   }

   void addProfilingTrace(
       const char* className,
       const char* fctName,
       TimeStamp start,
       TimeStamp end,
       uint8_t group )
   {
      _shallowTraces.push_back( ShallowTrace{ className, fctName, start, end, group } );
   }

   void addWaitLockTrace( void* mutexAddr, TimeStamp start, TimeStamp end )
   {
      _lockWaits.push_back( LockWait{ mutexAddr, start, end } );
   }

   uint32_t findOrAddStringToDb( const char* strId )
   {
      // Early return on NULL. The db should always contains NULL as first
      // entry
      if( strId == NULL ) return 0;

      auto nameIt = std::find( _nameArrayId.begin(), _nameArrayId.end(), strId );
      // If the string was not found, add it to the database and return its index
      if( nameIt == _nameArrayId.end() )
      {
         _nameArrayId.push_back( strId );
         const size_t newEntryPos = _nameArrayData.size();
         _nameArrayData.resize( _nameArrayData.size() + strlen( strId ) + 1 );
         strcpy( &_nameArrayData[newEntryPos], strId );
         return newEntryPos;
      }
      auto index = std::distance( _nameArrayId.begin(), nameIt );

      size_t nullCount = 0;
      size_t charIdx = 0;
      for( ; charIdx < _nameArrayData.size(); ++charIdx )
      {
         if( _nameArrayData[ charIdx ] == 0 )
         {
            if( ++nullCount == index ) break;
         }
      }

      assert( charIdx + 1 < _nameArrayData.size() );
      return charIdx + 1;
   }

   void flushToServer()
   {
      if( !ClientProfiler::sharedMemory.ok() )
      {
            return;
      }

      std::vector< std::pair< uint32_t, uint32_t > > classFctNamesIdx;
      classFctNamesIdx.reserve( _shallowTraces.size() );
      for( const auto& t : _shallowTraces  )
      {
         classFctNamesIdx.emplace_back(
             findOrAddStringToDb( t.className ), findOrAddStringToDb( t.fctName ) );
      }

      // Allocate raw buffer to send to server. It should be big enough for all
      // messages to fit.

      // 1- Get size of profiling traces message
      const uint32_t stringDataSize = _nameArrayData.size();
      assert( stringDataSize >= _sentStringDataSize );
      const uint32_t stringToSend = stringDataSize - _sentStringDataSize;
      const size_t profilerMsgSize =
          sizeof( MsgInfo ) + stringToSend + sizeof( Trace ) * _shallowTraces.size();

      // 2- Get size of lock messages
      const size_t lockMsgSize = sizeof( MsgInfo ) + _lockWaits.size() * sizeof( LockWait );

      // Allocate big enough buffer for all messages
      _bufferToSend.resize( sizeof( MsgHeader ) + profilerMsgSize + lockMsgSize );
      uint8_t* bufferPtr = _bufferToSend.data();

      // First fill buffer with header
      {
         MsgHeader* msgHeader = (MsgHeader*)bufferPtr;
         // TODO: Investigate if the truncation from size_t to uint32 is safe .. or not
         msgHeader->threadId = (uint32_t)_threadId;
         msgHeader->msgCount = 2;
      }

      bufferPtr += sizeof( MsgHeader );

      // Fill the buffer with the profiling trace message
      {
         // The data layout is as follow:
         // =========================================================
         // tracesInfo  = Profiler specific infos  - Information about the profiling traces sent
         // stringData  = String Data              - Array with all strings referenced by the traces
         // traceToSend = Traces                   - Array containing all of the traces
         MsgInfo* tracesInfo = (MsgInfo*)bufferPtr;
         char* stringData = (char*)( bufferPtr + sizeof( MsgInfo ) );
         Trace* traceToSend = (Trace*)( bufferPtr + sizeof( MsgInfo ) + stringToSend );

         tracesInfo->type = MsgInfoType::PROFILER_TRACE;
         tracesInfo->traces.stringDataSize = stringToSend;
         tracesInfo->traces.traceCount = (uint32_t)_shallowTraces.size();

         // Copy string data into its array
         memcpy( stringData, _nameArrayData.data() + _sentStringDataSize, stringToSend );

         // Copy trace information into buffer to send
         for ( size_t i = 0; i < _shallowTraces.size(); ++i )
         {
            auto& t = traceToSend[i];
            t.start = _shallowTraces[i].start;
            t.end = _shallowTraces[i].end;
            t.classNameIdx = classFctNamesIdx[i].first;
            t.fctNameIdx = classFctNamesIdx[i].second;
            t.group = _shallowTraces[i].group;
         }
      }

      // Increment pointer to new pos in buffer
      bufferPtr += profilerMsgSize;

      // Fill the buffer with the lock message
      {
         MsgInfo* lwInfo = (MsgInfo*)bufferPtr;
         lwInfo->type = MsgInfoType::PROFILER_WAIT_LOCK;
         lwInfo->lockwaits.count = (uint32_t)_lockWaits.size();
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      }

      _client.send( _bufferToSend.data(), _bufferToSend.size() );

      // Update sent array size
      _sentStringDataSize = stringDataSize;
      // Free the buffers
      _shallowTraces.clear();
      _lockWaits.clear();
      _bufferToSend.clear();
   }

   int _pushTraceLevel{0};
   size_t _threadId{0};
   std::vector< ShallowTrace > _shallowTraces;
   std::vector< LockWait > _lockWaits;
   std::vector< uint8_t > _bufferToSend;
   std::vector< const char* > _nameArrayId;
   std::vector< char > _nameArrayData;
   uint32_t _sentStringDataSize{0}; // The size of the string array on the server side
};

ClientProfiler::Impl* ClientProfiler::Get( size_t threadId, bool createIfMissing /*= true*/ )
{
   if( !ClientProfiler::sharedMemory.ok() )
   {
      ClientProfiler::sharedMemory.create( "/tmp/vdbg_shared_mem", VDBG_SHARED_MEM_SIZE );
   }

   int tIndex = 0;
   int firstEmptyIdx = -1;
   for ( ; tIndex < MAX_THREAD_NB; ++tIndex )
   {
      // Find existing client profiler for current thread.
      if ( threadsId[tIndex] == threadId ) break;
      if ( firstEmptyIdx == -1 && threadsId[tIndex] == 0 ) firstEmptyIdx = tIndex;
   }

   // TODO: Hack, there is a potential race condition if 2 threads get the same index and
   // create a new client. Maybe use something like atomic index?

   // If we have not found any existing client profiler for the current thread,
   // lets create one.
   if ( tIndex >= MAX_THREAD_NB )
   {
      if( !createIfMissing ) return nullptr;

      assert( firstEmptyIdx < MAX_THREAD_NB ); // Maximum client profiler reached (one per thread) 
      tIndex = firstEmptyIdx;
      threadsId[tIndex] = threadId;
      clientProfilers[tIndex] = new ClientProfiler::Impl(threadId);
   }

   return clientProfilers[tIndex];
}

void ClientProfiler::StartProfile( ClientProfiler::Impl* impl )
{
   ++impl->_pushTraceLevel;
}

void ClientProfiler::EndProfile(
    ClientProfiler::Impl* impl,
    const char* name,
    const char* classStr,
    TimeStamp start,
    TimeStamp end,
    uint8_t group )
{
   const int remainingPushedTraces = --impl->_pushTraceLevel;
   impl->addProfilingTrace( classStr, name, start, end, group );
   if ( remainingPushedTraces <= 0 )
   {
      impl->flushToServer();
   }
}

void ClientProfiler::EndLockWait( Impl* impl, void* mutexAddr, TimeStamp start, TimeStamp end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code
   if( impl->_pushTraceLevel > 0 )
      impl->addWaitLockTrace( mutexAddr, start, end );
}

} // end of namespace details
} // end of namespace vdbg

// symbols to be acessed from shared library

// void* vdbg_details_start_wait_lock( size_t threadId )
// {
//    auto client = vdbg::details::ClientProfiler::Get( threadId, false );
//    if( client )
//    {
//       printf( "increase push trace lvl\n");
//       ++client->_pushTraceLevel;
//    }
//    return (void*) client;
// }

// void vdbg_details_end_wait_lock(
//     void* clientProfiler,
//     void* mutexAddr,
//     size_t timeStampStart,
//     size_t timeStampEnd )
// {
//    if( clientProfiler )
//    {
//       auto client = reinterpret_cast<vdbg::details::ClientProfiler::Impl*>( clientProfiler );
//       --client->_pushTraceLevel;
//       client->addWaitLockTrace( mutexAddr, timeStampStart, timeStampEnd );
//       printf( "decrease push trace lvl\n");
//    }
// }



/****************************************************************
                     Third Parties
****************************************************************/


//  Start of ringbuf.c by Mindaugas Rasiukevicius ===================
/*
 * Copyright (c) 2016-2017 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

/*
 * Atomic multi-producer single-consumer ring buffer, which supports
 * contiguous range operations and which can be conveniently used for
 * message passing.
 *
 * There are three offsets -- think of clock hands:
 * - NEXT: marks the beginning of the available space,
 * - WRITTEN: the point up to which the data is actually written.
 * - Observed READY: point up to which data is ready to be written.
 *
 * Producers
 *
 * Observe and save the 'next' offset, then request N bytes from
 * the ring buffer by atomically advancing the 'next' offset.  Once
 * the data is written into the "reserved" buffer space, the thread
 * clears the saved value; these observed values are used to compute
 * the 'ready' offset.
 *
 * Consumer
 *
 * Writes the data between 'written' and 'ready' offsets and updates
 * the 'written' value.  The consumer thread scans for the lowest
 * seen value by the producers.
 *
 * Key invariant
 *
 * Producers cannot go beyond the 'written' offset; producers are
 * also not allowed to catch up with the consumer.  Only the consumer
 * is allowed to catch up with the producer i.e. set the 'written'
 * offset to be equal to the 'next' offset.
 *
 * Wrap-around
 *
 * If the producer cannot acquire the requested length due to little
 * available space at the end of the buffer, then it will wraparound.
 * WRAP_LOCK_BIT in 'next' offset is used to lock the 'end' offset.
 *
 * There is an ABA problem if one producer stalls while a pair of
 * producer and consumer would both successfully wrap-around and set
 * the 'next' offset to the stale value of the first producer, thus
 * letting it to perform a successful CAS violating the invariant.
 * A counter in the 'next' offset (masked by WRAP_COUNTER) is used
 * to prevent from this problem.  It is incremented on wraparounds.
 *
 * The same ABA problem could also cause a stale 'ready' offset,
 * which could be observed by the consumer.  We set WRAP_LOCK_BIT in
 * the 'seen' value before advancing the 'next' and clear this bit
 * after the successful advancing; this ensures that only the stable
 * 'ready' observed by the consumer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


// =========== Start of Utils.h ===========
/*
 * Copyright (c) 1991, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)cdefs.h 8.8 (Berkeley) 1/9/95
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <assert.h>

/*
 * A regular assert (debug/diagnostic only).
 */
#if defined(DEBUG)
#define  ASSERT      assert
#else
#define  ASSERT(x)
#endif

/*
 * Minimum, maximum and rounding macros.
 */

#ifndef MIN
#define  MIN(x, y)   ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define  MAX(x, y)   ((x) > (y) ? (x) : (y))
#endif

/*
 * Branch prediction macros.
 */
#ifndef __predict_true
#define  __predict_true(x) __builtin_expect((x) != 0, 1)
#define  __predict_false(x)   __builtin_expect((x) != 0, 0)
#endif

/*
 * Atomic operations and memory barriers.  If C11 API is not available,
 * then wrap the GCC builtin routines.
 */
#ifndef atomic_compare_exchange_weak
#define  atomic_compare_exchange_weak(ptr, expected, desired) \
    __sync_bool_compare_and_swap(ptr, expected, desired)
#endif

#ifndef atomic_thread_fence
#define  memory_order_acquire __ATOMIC_ACQUIRE  // load barrier
#define  memory_order_release __ATOMIC_RELEASE  // store barrier
#define  atomic_thread_fence(m)  __atomic_thread_fence(m)
#endif

/*
 * Exponential back-off for the spinning paths.
 */
#define  SPINLOCK_BACKOFF_MIN 4
#define  SPINLOCK_BACKOFF_MAX 128
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_BACKOFF_HOOK __asm volatile("pause" ::: "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define  SPINLOCK_BACKOFF(count)             \
do {                       \
   for (int __i = (count); __i != 0; __i--) {      \
      SPINLOCK_BACKOFF_HOOK;           \
   }                    \
   if ((count) < SPINLOCK_BACKOFF_MAX)       \
      (count) += (count);           \
} while (/* CONSTCOND */ 0);

#endif

// =========== end of Utils.h ===========

#define  RBUF_OFF_MASK  (0x00000000ffffffffUL)
#define  WRAP_LOCK_BIT  (0x8000000000000000UL)
#define  RBUF_OFF_MAX   (UINT64_MAX & ~WRAP_LOCK_BIT)

#define  WRAP_COUNTER   (0x7fffffff00000000UL)
#define  WRAP_INCR(x)   (((x) + 0x100000000UL) & WRAP_COUNTER)

typedef uint64_t  ringbuf_off_t;

struct ringbuf_worker {
   volatile ringbuf_off_t  seen_off;
   int         registered;
};

struct ringbuf {
   /* Ring buffer space. */
   size_t         space;

   /*
    * The NEXT hand is atomically updated by the producer.
    * WRAP_LOCK_BIT is set in case of wrap-around; in such case,
    * the producer can update the 'end' offset.
    */
   volatile ringbuf_off_t  next;
   ringbuf_off_t     end;

   /* The following are updated by the consumer. */
   ringbuf_off_t     written;
   unsigned    nworkers;
   ringbuf_worker_t  workers[];
};

/*
 * ringbuf_setup: initialise a new ring buffer of a given length.
 */
int
ringbuf_setup(ringbuf_t *rbuf, unsigned nworkers, size_t length)
{
   if (length >= RBUF_OFF_MASK) {
      errno = EINVAL;
      return -1;
   }
   memset(rbuf, 0, sizeof(ringbuf_t));
   rbuf->space = length;
   rbuf->end = RBUF_OFF_MAX;
   rbuf->nworkers = nworkers;
   return 0;
}

/*
 * ringbuf_get_sizes: return the sizes of the ringbuf_t and ringbuf_worker_t.
 */
void
ringbuf_get_sizes(const unsigned nworkers,
    size_t *ringbuf_size, size_t *ringbuf_worker_size)
{
   if (ringbuf_size)
      *ringbuf_size = offsetof(ringbuf_t, workers[nworkers]);
   if (ringbuf_worker_size)
      *ringbuf_worker_size = sizeof(ringbuf_worker_t);
}

/*
 * ringbuf_register: register the worker (thread/process) as a producer
 * and pass the pointer to its local store.
 */
ringbuf_worker_t *
ringbuf_register(ringbuf_t *rbuf, unsigned i)
{
   ringbuf_worker_t *w = &rbuf->workers[i];

   w->seen_off = RBUF_OFF_MAX;
   atomic_thread_fence(memory_order_release);
   w->registered = true;
   return w;
}

void
ringbuf_unregister(ringbuf_t *rbuf, ringbuf_worker_t *w)
{
   w->registered = false;
   (void)rbuf;
}

/*
 * stable_nextoff: capture and return a stable value of the 'next' offset.
 */
static inline ringbuf_off_t
stable_nextoff(ringbuf_t *rbuf)
{
   unsigned count = SPINLOCK_BACKOFF_MIN;
   ringbuf_off_t next;

   while ((next = rbuf->next) & WRAP_LOCK_BIT) {
      SPINLOCK_BACKOFF(count);
   }
   atomic_thread_fence(memory_order_acquire);
   ASSERT((next & RBUF_OFF_MASK) < rbuf->space);
   return next;
}

/*
 * ringbuf_acquire: request a space of a given length in the ring buffer.
 *
 * => On success: returns the offset at which the space is available.
 * => On failure: returns -1.
 */
ssize_t
ringbuf_acquire(ringbuf_t *rbuf, ringbuf_worker_t *w, size_t len)
{
   ringbuf_off_t seen, next, target;

   ASSERT(len > 0 && len <= rbuf->space);
   ASSERT(w->seen_off == RBUF_OFF_MAX);

   do {
      ringbuf_off_t written;

      /*
       * Get the stable 'next' offset.  Save the observed 'next'
       * value (i.e. the 'seen' offset), but mark the value as
       * unstable (set WRAP_LOCK_BIT).
       *
       * Note: CAS will issue a memory_order_release for us and
       * thus ensures that it reaches global visibility together
       * with new 'next'.
       */
      seen = stable_nextoff(rbuf);
      next = seen & RBUF_OFF_MASK;
      ASSERT(next < rbuf->space);
      w->seen_off = next | WRAP_LOCK_BIT;

      /*
       * Compute the target offset.  Key invariant: we cannot
       * go beyond the WRITTEN offset or catch up with it.
       */
      target = next + len;
      written = rbuf->written;
      if (__predict_false(next < written && target >= written)) {
         /* The producer must wait. */
         w->seen_off = RBUF_OFF_MAX;
         return -1;
      }

      if (__predict_false(target >= rbuf->space)) {
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
         target = exceed ? (WRAP_LOCK_BIT | len) : 0;
         if ((target & RBUF_OFF_MASK) >= written) {
            w->seen_off = RBUF_OFF_MAX;
            return -1;
         }
         /* Increment the wrap-around counter. */
         target |= WRAP_INCR(seen & WRAP_COUNTER);
      } else {
         /* Preserve the wrap-around counter. */
         target |= seen & WRAP_COUNTER;
      }
   } while (!atomic_compare_exchange_weak(&rbuf->next, seen, target));

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
   if (__predict_false(target & WRAP_LOCK_BIT)) {
      /* Cannot wrap-around again if consumer did not catch-up. */
      ASSERT(rbuf->written <= next);
      ASSERT(rbuf->end == RBUF_OFF_MAX);
      rbuf->end = next;
      next = 0;

      /*
       * Unlock: ensure the 'end' offset reaches global
       * visibility before the lock is released.
       */
      atomic_thread_fence(memory_order_release);
      rbuf->next = (target & ~WRAP_LOCK_BIT);
   }
   ASSERT((target & RBUF_OFF_MASK) <= rbuf->space);
   return (ssize_t)next;
}

/*
 * ringbuf_produce: indicate the acquired range in the buffer is produced
 * and is ready to be consumed.
 */
void
ringbuf_produce(ringbuf_t *rbuf, ringbuf_worker_t *w)
{
   (void)rbuf;
   ASSERT(w->registered);
   ASSERT(w->seen_off != RBUF_OFF_MAX);
   atomic_thread_fence(memory_order_release);
   w->seen_off = RBUF_OFF_MAX;
}

/*
 * ringbuf_consume: get a contiguous range which is ready to be consumed.
 */
size_t
ringbuf_consume(ringbuf_t *rbuf, size_t *offset)
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
   next = stable_nextoff(rbuf) & RBUF_OFF_MASK;
   if (written == next) {
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

   for (unsigned i = 0; i < rbuf->nworkers; i++) {
      ringbuf_worker_t *w = &rbuf->workers[i];
      unsigned count = SPINLOCK_BACKOFF_MIN;
      ringbuf_off_t seen_off;

      /* Skip if the worker has not registered. */
      if (!w->registered) {
         continue;
      }

      /*
       * Get a stable 'seen' value.  This is necessary since we
       * want to discard the stale 'seen' values.
       */
      while ((seen_off = w->seen_off) & WRAP_LOCK_BIT) {
         SPINLOCK_BACKOFF(count);
      }

      /*
       * Ignore the offsets after the possible wrap-around.
       * We are interested in the smallest seen offset that is
       * not behind the 'written' offset.
       */
      if (seen_off >= written) {
         ready = MIN(seen_off, ready);
      }
      ASSERT(ready >= written);
   }

   /*
    * Finally, we need to determine whether wrap-around occurred
    * and deduct the safe 'ready' offset.
    */
   if (next < written) {
      const ringbuf_off_t end = MIN(rbuf->space, rbuf->end);

      /*
       * Wrap-around case.  Check for the cut off first.
       *
       * Reset the 'written' offset if it reached the end of
       * the buffer or the 'end' offset (if set by a producer).
       * However, we must check that the producer is actually
       * done (the observed 'ready' offsets are clear).
       */
      if (ready == RBUF_OFF_MAX && written == end) {
         /*
          * Clear the 'end' offset if was set.
          */
         if (rbuf->end != RBUF_OFF_MAX) {
            rbuf->end = RBUF_OFF_MAX;
            atomic_thread_fence(memory_order_release);
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
      ASSERT(ready > next);
      ready = MIN(ready, end);
      ASSERT(ready >= written);
   } else {
      /*
       * Regular case.  Up to the observed 'ready' (if set)
       * or the 'next' offset.
       */
      ready = MIN(ready, next);
   }
   towrite = ready - written;
   *offset = written;

   ASSERT(ready >= written);
   ASSERT(towrite <= rbuf->space);
   return towrite;
}

/*
 * ringbuf_release: indicate that the consumed range can now be released.
 */
void
ringbuf_release(ringbuf_t *rbuf, size_t nbytes)
{
   const size_t nwritten = rbuf->written + nbytes;

   ASSERT(rbuf->written <= rbuf->space);
   ASSERT(rbuf->written <= rbuf->end);
   ASSERT(nwritten <= rbuf->space);

   rbuf->written = (nwritten == rbuf->space) ? 0 : nwritten;
}

// End of ringbuf.c by Mindaugas Rasiukevicius ===================

#endif  // end VDBG_IMPLEMENTATION

#endif  // !defined(VDBG_ENABLED)

#endif  // VDBG_H_