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

#include <atomic>
#include <memory>
#include <chrono>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>

// ------ platform.h ------------
// This is most things that are potentially non-portable.
#define VDBG_CONSTEXPR constexpr
#define VDBG_NOEXCEPT noexcept
#define VDBG_STATIC_ASSERT static_assert
#define VDBG_GET_THREAD_ID() (size_t)pthread_self()
// On MacOs the max name length seems to be 30...
#define VDBG_SHARED_MEM_MAX_NAME_SIZE 30
#define VDBG_SHARED_MEM_PREFIX "/vdbg_"
extern char* __progname;
inline const char* getProgName() VDBG_NOEXCEPT
{
   return __progname;
}
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
// -----------------------------

// Forward declarations of type used by ringbuffer as adapted from
// Mindaugas Rasiukevicius. See below for Copyright/Disclaimer
typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;

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

enum class MsgType : uint32_t
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
   MsgType type;
   // Thread id from which the msg was sent
   uint32_t threadId;
   // Specific message data
   union {
      TracesMsgInfo traces;
      LockWaitsMsgInfo lockwaits;
   };
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

// ------ SharedMemory.h ------------
class SharedMemory
{
  public:
   bool create( const char* path, size_t size, bool isConsumer );
   void destroy();

   struct SharedMetaInfo
   {
      enum Flags
      {
         CONNECTED_CONSUMER = 1 << 0,
         LISTENING_CONSUMER = 1 << 1,
      };
      std::atomic< uint32_t > flags{0};
   };

   bool hasConnectedConsumer() const VDBG_NOEXCEPT;
   void setConnectedConsumer( bool ) VDBG_NOEXCEPT;
   bool hasListeningConsumer() const VDBG_NOEXCEPT;
   void setListeningConsumer( bool ) VDBG_NOEXCEPT;
   ringbuf_t* ringbuffer() const VDBG_NOEXCEPT;
   uint8_t* data() const VDBG_NOEXCEPT;
   sem_t* semaphore() const VDBG_NOEXCEPT;
   const SharedMetaInfo* sharedMetaInfo() const VDBG_NOEXCEPT;
   ~SharedMemory();

  private:
   // Pointer into the shared memory
   SharedMetaInfo* _sharedMetaData{NULL};
   ringbuf_t* _ringbuf{NULL};
   uint8_t* _data{NULL};
   // ----------------
   sem_t* _semaphore{NULL};
   bool _isConsumer;
   size_t _size{0};
   int _sharedMemFd{-1};
   char _sharedMemPath[VDBG_SHARED_MEM_MAX_NAME_SIZE];
};
// ------ end of SharedMemory.h ------------

static constexpr int MAX_THREAD_NB = 64;
class Client;
class ClientManager
{
  public:
   static Client* Get();
   static void StartProfile();
   static void EndProfile(
       const char* name,
       const char* classStr,
       TimeStamp start,
       TimeStamp end,
       uint8_t group );
   static void EndLockWait(
      void* mutexAddr,
      TimeStamp start,
      TimeStamp end );
   static bool HasConnectedConsumer() VDBG_NOEXCEPT;
   static bool HasListeningConsumer() VDBG_NOEXCEPT;

   static SharedMemory sharedMemory;
};

class ProfGuard
{
  public:
   ProfGuard( const char* name, const char* classStr, uint8_t groupId ) VDBG_NOEXCEPT
       : start( getTimeStamp() ),
         className( classStr ),
         fctName( name ),
         group( groupId )
   {
      ClientManager::StartProfile();
   }
   ~ProfGuard()
   {
      ClientManager::EndProfile( fctName, className, start, getTimeStamp(), group );
   }

  private:
   TimeStamp start;
   const char *className, *fctName;
   uint8_t group;
};

struct LockWaitGuard
{
   LockWaitGuard( void* mutAddr )
       : start( getTimeStamp() ),
         mutexAddr( mutAddr )
   {
   }
   ~LockWaitGuard()
   {
      ClientManager::EndLockWait( mutexAddr, start, getTimeStamp() );
   }

   TimeStamp start;
   void* mutexAddr;
};

#define VDBG_COMBINE( X, Y ) X##Y
#define VDBG_PROF_GUARD_VAR( LINE, ARGS ) \
   vdbg::details::ProfGuard VDBG_COMBINE( vdbgProfGuard, LINE ) ARGS

}  // namespace details
}  // namespace vdbg



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
#include <unistd.h>

int ringbuf_setup( ringbuf_t*, unsigned, size_t );
void ringbuf_get_sizes( unsigned, size_t*, size_t* );

ringbuf_worker_t* ringbuf_register( ringbuf_t*, unsigned );
void ringbuf_unregister( ringbuf_t*, ringbuf_worker_t* );

ssize_t ringbuf_acquire( ringbuf_t*, ringbuf_worker_t*, size_t );
void ringbuf_produce( ringbuf_t*, ringbuf_worker_t* );
size_t ringbuf_consume( ringbuf_t*, size_t* );
void ringbuf_release( ringbuf_t*, size_t );

/* ====================================================================== */

// End of vdbg declarations

#if defined(VDBG_IMPLEMENTATION) || defined(VDBG_SERVER_IMPLEMENTATION)

// C includes
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <unistd.h>

// C++ standard includes
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <vector>

namespace vdbg
{
namespace details
{

// ------ SharedMemory.cpp------------
bool SharedMemory::create( const char* path, size_t requestedSize, bool isConsumer )
{
   _isConsumer = isConsumer;
   // Get the size needed for the ringbuf struct
   size_t ringBufSize;
   ringbuf_get_sizes(MAX_THREAD_NB, &ringBufSize, NULL);

   // TODO handle signals
   // signal( SIGINT, sig_callback_handler );
   strncpy( _sharedMemPath, path, VDBG_SHARED_MEM_MAX_NAME_SIZE - 1 );
   _size = requestedSize;

   _sharedMemFd = shm_open( path, O_CREAT | O_RDWR, 0666 );
   if ( _sharedMemFd < 0 )
   {
      if( !isConsumer )
      {
         perror( "Could not shm_open shared memory" );
      }
      // In the case of a consumer, this simply means that there is no producer to read from.
      return false;
   }

   const size_t totalSize = ringBufSize + requestedSize + sizeof( SharedMetaInfo );
   ftruncate( _sharedMemFd, totalSize );

   uint8_t* sharedMem = (uint8_t*) mmap( NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, _sharedMemFd, 0 );
   if ( sharedMem == NULL )
   {
      perror( "Could not map shared memory" );
      return false;
   }

   // Get pointers inside the shared memoryu
   _sharedMetaData = (SharedMetaInfo*) sharedMem;
   _ringbuf = (ringbuf_t*) (sharedMem + sizeof( SharedMetaInfo ));
   _data = sharedMem + sizeof( SharedMetaInfo ) + ringBufSize ;

   // Reset everything but the metadata if we are a producer. (The metadata
   // could still be in used  by a server when we get here, if we start
   // the server first for example)
   if( !isConsumer )
   {
      memset( _ringbuf, 0, totalSize - sizeof( SharedMetaInfo) );
   }

   if( ringbuf_setup( _ringbuf, MAX_THREAD_NB, requestedSize ) < 0 )
   {
      assert( false && "Ring buffer creation failed" );
   }

   // We can only have one consumer
   if( isConsumer && hasConnectedConsumer() )
   {
      printf("Cannot have more than one instance of the consumer at a time."
             " You might be trying to run the consumer application twice or"
             " have a dangling shared memory segment.\n");
      _sharedMemFd = -1;
      _data = NULL;
      _sharedMetaData = NULL;
      _ringbuf = NULL;
      exit(-1);
   }

   // Open semaphore
   _semaphore = sem_open( "/mysem", O_CREAT, S_IRUSR | S_IWUSR, 1 );

   if( isConsumer )
   {
      _sharedMetaData->flags |= SharedMetaInfo::CONNECTED_CONSUMER;
   }

   return true;
}

bool SharedMemory::hasConnectedConsumer() const VDBG_NOEXCEPT
{
   const uint32_t mask = SharedMetaInfo::CONNECTED_CONSUMER;
   return (sharedMetaInfo()->flags) & mask == mask;
}

void SharedMemory::setConnectedConsumer( bool connected ) VDBG_NOEXCEPT
{
   if( connected )
      _sharedMetaData->flags |= SharedMetaInfo::CONNECTED_CONSUMER;
   else
      _sharedMetaData->flags &= ~SharedMetaInfo::CONNECTED_CONSUMER;

}

bool SharedMemory::hasListeningConsumer() const VDBG_NOEXCEPT
{
   const uint32_t mask = SharedMetaInfo::CONNECTED_CONSUMER | SharedMetaInfo::LISTENING_CONSUMER;
   return (sharedMetaInfo()->flags & mask) == mask;
}

void SharedMemory::setListeningConsumer( bool listening ) VDBG_NOEXCEPT
{
   if(listening)
      _sharedMetaData->flags |= SharedMetaInfo::LISTENING_CONSUMER;
   else
      _sharedMetaData->flags &= ~(SharedMetaInfo::LISTENING_CONSUMER);

}

uint8_t* SharedMemory::data() const VDBG_NOEXCEPT
{
   return _data;
}

ringbuf_t* SharedMemory::ringbuffer() const VDBG_NOEXCEPT
{
   return _ringbuf;
}

sem_t* SharedMemory::semaphore() const VDBG_NOEXCEPT
{
   return _semaphore;
}

const SharedMemory::SharedMetaInfo* SharedMemory::sharedMetaInfo() const VDBG_NOEXCEPT
{
   return _sharedMetaData;
}

void SharedMemory::destroy()
{
   if ( data() )
   {
      if ( _semaphore && sem_close( _semaphore ) != 0 )
      {
         perror( "Could not close semaphore" );
      }

      if ( _semaphore && sem_unlink("/mysem") < 0 )
      {
         perror( "Could not unlink semaphore" );
      }

      if( _isConsumer )
         _sharedMetaData->flags &= ~(SharedMetaInfo::CONNECTED_CONSUMER | SharedMetaInfo::LISTENING_CONSUMER);

      if ( _sharedMemPath && shm_unlink( _sharedMemPath ) != 0 )
      {
         perror( "Could not unlink shared memory" );
      }

      _data = NULL;
      _semaphore = NULL;
      _ringbuf = NULL;
   }
}

SharedMemory::~SharedMemory()
{
   destroy();
}
// ------ end of SharedMemory.cpp------------

// Following is the impelementation specific to client side (not server side)
#ifndef VDBG_SERVER_IMPLEMENTATION

// ------ cdbg_client.cpp------------

// The shared memory that will be created by the client process to communicate
// with the server
SharedMemory ClientManager::sharedMemory;

// The call stack depth of the current measured trace. One variable per thread
thread_local int tl_traceLevel = 0;
thread_local size_t tl_threadId = 0;

class Client
{
   struct ShallowTrace
   {
      const char *className, *fctName;
      TimeStamp start, end;
      uint8_t group;
   };

  public:
   Client()
   {
      _shallowTraces.reserve( 256 );
      _lockWaits.reserve( 256 );
      _stringIndex.reserve( 256 );
      _stringData.reserve( 256 * 32 );

      // Push back first name as empty string
      _stringIndex[ NULL ] = 0;
      _stringData.push_back('\0');
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

      auto& stringIndex = _stringIndex[strId];
      // If the string was not found, add it to the database and return its index
      if( stringIndex == 0 )
      {
         const size_t newEntryPos = _stringData.size();
         stringIndex = newEntryPos;
         _stringData.resize( newEntryPos + strlen( strId ) + 1 );
         strcpy( &_stringData[newEntryPos], strId );
         return newEntryPos;
      }

      return stringIndex;
   }

   void flushToConsumer()
   {
      if( !ClientManager::HasListeningConsumer() )
      {
         _shallowTraces.clear();
         _lockWaits.clear();
         // Also reset the string data that was sent since we might
         // have lost the connection with the consumer
         _sentStringDataSize = 0;
         return;
      }

      // Convert string pointers to index in the string database
      std::vector< std::pair< uint32_t, uint32_t > > classFctNamesIdx;
      classFctNamesIdx.reserve( _shallowTraces.size() );
      for( const auto& t : _shallowTraces  )
      {
         classFctNamesIdx.emplace_back(
             findOrAddStringToDb( t.className ), findOrAddStringToDb( t.fctName ) );
      }

      // 1- Get size of profiling traces message
      const uint32_t stringDataSize = _stringData.size();
      assert( stringDataSize >= _sentStringDataSize );
      const uint32_t stringToSend = stringDataSize - _sentStringDataSize;
      const size_t profilerMsgSize =
          sizeof( MsgInfo ) + stringToSend + sizeof( Trace ) * _shallowTraces.size();

      // Allocate big enough buffer from the shared memory
      ringbuf_t* ringbuf = ClientManager::sharedMemory.ringbuffer();
      const auto offset = ringbuf_acquire( ringbuf, _worker, profilerMsgSize );
      if ( offset == -1 )
      {
         printf("Failed to acquire enough shared memory. Consider increasing shared memory size\n");
         _shallowTraces.clear();
         return;
      }

      uint8_t* bufferPtr = &ClientManager::sharedMemory.data()[offset];

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

         tracesInfo->type = MsgType::PROFILER_TRACE;
         tracesInfo->threadId = (uint32_t)tl_threadId;
         tracesInfo->traces.stringDataSize = stringToSend;
         tracesInfo->traces.traceCount = (uint32_t)_shallowTraces.size();

         // Copy string data into its array
         memcpy( stringData, _stringData.data() + _sentStringDataSize, stringToSend );

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

      ringbuf_produce( ringbuf, _worker );
      sem_post( ClientManager::sharedMemory.semaphore() );

      // Update sent array size
      _sentStringDataSize = stringDataSize;
      // Free the buffers
      _shallowTraces.clear();


      // // Increment pointer to new pos in buffer
      // bufferPtr += profilerMsgSize;

      //       // 2- Get size of lock messages
      // const size_t lockMsgSize = sizeof( MsgInfo ) + _lockWaits.size() * sizeof( LockWait );
      // // Fill the buffer with the lock message
      // {
      //    MsgInfo* lwInfo = (MsgInfo*)bufferPtr;
      //    lwInfo->type = MsgType::PROFILER_WAIT_LOCK;
      //    lwInfo->lockwaits.count = (uint32_t)_lockWaits.size();
      //    bufferPtr += sizeof( MsgInfo );
      //    memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      // }

      // ringbuf_t* ringbuf = ClientManager::sharedMemory.ringbuffer();
      // uint8_t* mem = ClientManager::sharedMemory.data();
      
      // // Get buffer from shared memory
      // static size_t msgCount=0;
      // auto offset = ringbuf_acquire( ringbuf, _worker, sizeof( "Hello world" ) );
      // if( offset != -1 )
      // {
      //    memcpy( (void*)&mem[offset], (void*)"Hello world", sizeof("Hello world" ) );
      //    ringbuf_produce( ringbuf, _worker);
      //    sem_post( ClientManager::sharedMemory._semaphore );
      //    printf("Message sent!%lu\n", ++msgCount );
      // }

      _lockWaits.clear();
      //_bufferToSend.clear();
   }

   std::vector< ShallowTrace > _shallowTraces;
   std::vector< LockWait > _lockWaits;
   std::unordered_map< const char*, size_t > _stringIndex;
   std::vector< char > _stringData;
   ringbuf_worker_t* _worker{NULL};
   uint32_t _sentStringDataSize{0}; // The size of the string array on the server side
};

Client* ClientManager::Get()
{
   thread_local std::unique_ptr< Client > threadClient;

   if( likely( threadClient.get() ) ) return threadClient.get();

   // If we have not yet created our shared memory segment, do it here
   if( !ClientManager::sharedMemory.data() )
   {
      char path[VDBG_SHARED_MEM_MAX_NAME_SIZE] = {};
      strncpy( path, VDBG_SHARED_MEM_PREFIX, sizeof( VDBG_SHARED_MEM_PREFIX ) );
      strncat(
          path, __progname, VDBG_SHARED_MEM_MAX_NAME_SIZE - sizeof( VDBG_SHARED_MEM_PREFIX ) - 1 );
      bool sucess = ClientManager::sharedMemory.create( path, VDBG_SHARED_MEM_SIZE, false );
      assert( sucess && "Could not create shared memory" );
   }

   static int threadCount = 0;
   ++threadCount;
   assert( threadCount <= MAX_THREAD_NB );
   tl_threadId = VDBG_GET_THREAD_ID();
   threadClient.reset( new Client() );

   // Register producer in the ringbuffer
   auto ringBuffer = ClientManager::sharedMemory.ringbuffer();
   threadClient->_worker = ringbuf_register( ringBuffer, threadCount );
   if ( threadClient->_worker  == NULL )
   {
      assert( false && "ringbuf_register" );
   } 

   return threadClient.get();
}

void ClientManager::StartProfile()
{
   ++tl_traceLevel;
}

void ClientManager::EndProfile(
    const char* name,
    const char* classStr,
    TimeStamp start,
    TimeStamp end,
    uint8_t group )
{
   const int remainingPushedTraces = --tl_traceLevel;
   Client* client = ClientManager::Get();
   client->addProfilingTrace( classStr, name, start, end, group );
   if ( remainingPushedTraces <= 0 )
   {
      client->flushToConsumer();
   }
}

void ClientManager::EndLockWait( void* mutexAddr, TimeStamp start, TimeStamp end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code
   if( tl_traceLevel > 0 )
   {
      ClientManager::Get()->addWaitLockTrace( mutexAddr, start, end );
   }
}

bool ClientManager::HasConnectedConsumer() VDBG_NOEXCEPT
{
   return ClientManager::sharedMemory.data() &&
          ClientManager::sharedMemory.hasConnectedConsumer();
}

bool ClientManager::HasListeningConsumer() VDBG_NOEXCEPT
{
   return ClientManager::sharedMemory.data() &&
          ClientManager::sharedMemory.hasListeningConsumer();
}

#endif  // end !VDBG_SERVER_IMPLEMENTATION

} // end of namespace details
} // end of namespace vdbg


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <algorithm>

/*  Utils.h */

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
#define SPINLOCK_BACKOFF( count )                                     \
   do                                                                 \
   {                                                                  \
      for ( int __i = ( count ); __i != 0; __i-- )                    \
      {                                                               \
         SPINLOCK_BACKOFF_HOOK;                                       \
      }                                                               \
      if ( ( count ) < SPINLOCK_BACKOFF_MAX ) ( count ) += ( count ); \
   } while ( /* CONSTCOND */ 0 );

/* end of Utils.h */

/* ringbuf.c */
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

struct ringbuf
{
   /* Ring buffer space. */
   size_t space;

   /*
    * The NEXT hand is atomically updated by the producer.
    * WRAP_LOCK_BIT is set in case of wrap-around; in such case,
    * the producer can update the 'end' offset.
    */
   std::atomic< ringbuf_off_t > next;
   ringbuf_off_t end;

   /* The following are updated by the consumer. */
   ringbuf_off_t written;
   unsigned nworkers;
   ringbuf_worker_t workers[];
};

/*
 * ringbuf_setup: initialise a new ring buffer of a given length.
 */
int ringbuf_setup( ringbuf_t* rbuf, unsigned nworkers, size_t length )
{
   if ( length >= RBUF_OFF_MASK )
   {
      errno = EINVAL;
      return -1;
   }
   memset( rbuf, 0, sizeof( ringbuf_t ) );
   rbuf->space = length;
   rbuf->end = RBUF_OFF_MAX;
   rbuf->nworkers = nworkers;
   return 0;
}

/*
 * ringbuf_get_sizes: return the sizes of the ringbuf_t and ringbuf_worker_t.
 */
void ringbuf_get_sizes( const unsigned nworkers, size_t* ringbuf_size, size_t* ringbuf_worker_size )
{
   if ( ringbuf_size ) *ringbuf_size = offsetof( ringbuf_t, workers[nworkers] );
   if ( ringbuf_worker_size ) *ringbuf_worker_size = sizeof( ringbuf_worker_t );
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

void ringbuf_unregister( ringbuf_t* rbuf, ringbuf_worker_t* w )
{
   w->registered = false;
   (void)rbuf;
}

/*
 * stable_nextoff: capture and return a stable value of the 'next' offset.
 */
static inline ringbuf_off_t stable_nextoff( ringbuf_t* rbuf )
{
   unsigned count = SPINLOCK_BACKOFF_MIN;
   ringbuf_off_t next;

   while ( ( next = rbuf->next ) & WRAP_LOCK_BIT )
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
      target = next + len;
      written = rbuf->written;
      if ( unlikely( next < written && target >= written ) )
      {
         /* The producer must wait. */
         w->seen_off = RBUF_OFF_MAX;
         return -1;
      }

      if ( unlikely( target >= rbuf->space ) )
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
         if ( ( target & RBUF_OFF_MASK ) >= written )
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
   } while ( !std::atomic_compare_exchange_weak( &rbuf->next, &seen, target ) );

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
   if ( unlikely( target & WRAP_LOCK_BIT ) )
   {
      /* Cannot wrap-around again if consumer did not catch-up. */
      assert( rbuf->written <= next );
      assert( rbuf->end == RBUF_OFF_MAX );
      rbuf->end = next;
      next = 0;

      /*
       * Unlock: ensure the 'end' offset reaches global
       * visibility before the lock is released.
       */
      std::atomic_thread_fence( std::memory_order_release );
      rbuf->next = ( target & ~WRAP_LOCK_BIT );
   }
   assert( ( target & RBUF_OFF_MASK ) <= rbuf->space );
   return (ssize_t)next;
}

/*
 * ringbuf_produce: indicate the acquired range in the buffer is produced
 * and is ready to be consumed.
 */
void ringbuf_produce( ringbuf_t* rbuf, ringbuf_worker_t* w )
{
   (void)rbuf;
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
   if ( written == next )
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

   for ( unsigned i = 0; i < rbuf->nworkers; i++ )
   {
      ringbuf_worker_t* w = &rbuf->workers[i];
      unsigned count = SPINLOCK_BACKOFF_MIN;
      ringbuf_off_t seen_off;

      /* Skip if the worker has not registered. */
      if ( !w->registered )
      {
         continue;
      }

      /*
       * Get a stable 'seen' value.  This is necessary since we
       * want to discard the stale 'seen' values.
       */
      while ( ( seen_off = w->seen_off ) & WRAP_LOCK_BIT )
      {
         SPINLOCK_BACKOFF( count );
      }

      /*
       * Ignore the offsets after the possible wrap-around.
       * We are interested in the smallest seen offset that is
       * not behind the 'written' offset.
       */
      if ( seen_off >= written )
      {
         ready = std::min( seen_off, ready );
      }
      assert( ready >= written );
   }

   /*
    * Finally, we need to determine whether wrap-around occurred
    * and deduct the safe 'ready' offset.
    */
   if ( next < written )
   {
      const ringbuf_off_t end = std::min( (ringbuf_off_t) rbuf->space, rbuf->end );

      /*
       * Wrap-around case.  Check for the cut off first.
       *
       * Reset the 'written' offset if it reached the end of
       * the buffer or the 'end' offset (if set by a producer).
       * However, we must check that the producer is actually
       * done (the observed 'ready' offsets are clear).
       */
      if ( ready == RBUF_OFF_MAX && written == end )
      {
         /*
          * Clear the 'end' offset if was set.
          */
         if ( rbuf->end != RBUF_OFF_MAX )
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
      ready = std::min( ready, end );
      assert( ready >= written );
   }
   else
   {
      /*
       * Regular case.  Up to the observed 'ready' (if set)
       * or the 'next' offset.
       */
      ready = std::min( ready, next );
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

/* end of ringbuf.c */

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

#endif  // end VDBG_IMPLEMENTATION

#endif  // !defined(VDBG_ENABLED)

#endif  // VDBG_H_