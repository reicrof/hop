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
#define VDBG_SHARED_MEM_NAME "/vdbg_shared_mem4"
extern char* __progname;
inline const char* getProgName() VDBG_NOEXCEPT
{
   return __progname;
}
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
   bool create( const char* path, size_t size, bool isProducer );
   void destroy();

   struct SharedMetaInfo
   {
      std::atomic< int > producerCount;
      std::atomic< int > consumerCount;
   };

   int consumerCount() const VDBG_NOEXCEPT;
   int producerCount() const VDBG_NOEXCEPT;
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
   size_t _size{0};
   const char* _path{NULL};
   int _sharedMemFd{-1};
   bool _isProducer;
};
// ------ end of SharedMemory.h ------------

static constexpr int MAX_THREAD_NB = 64;
class Client;
class ClientManager
{
  public:
   static Client* Get( size_t threadId, bool createIfMissing = true );
   static void StartProfile( Client* );
   static void EndProfile(
       Client*,
       const char* name,
       const char* classStr,
       TimeStamp start,
       TimeStamp end,
       uint8_t group );
   static void EndLockWait(
      Client*,
      void* mutexAddr,
      TimeStamp start,
      TimeStamp end );
   static bool HasConnectedServer() VDBG_NOEXCEPT;

   static SharedMemory sharedMemory;
   static size_t threadsId[MAX_THREAD_NB];
   static std::unique_ptr< Client > clients[MAX_THREAD_NB];
};

class ProfGuard
{
  public:
   ProfGuard( const char* name, const char* classStr, uint8_t groupId ) VDBG_NOEXCEPT
       : start( getTimeStamp() ),
         className( classStr ),
         fctName( name ),
         client( ClientManager::Get( VDBG_GET_THREAD_ID() ) ),
         group( groupId )
   {
      ClientManager::StartProfile( client );
   }
   ~ProfGuard()
   {
      end = getTimeStamp();
      ClientManager::EndProfile( client, fctName, className, start, end, group );
   }

  private:
   TimeStamp start, end;
   const char *className, *fctName;
   Client* client;
   uint8_t group;
};

struct LockWaitGuard
{
   LockWaitGuard( void* mutAddr )
       : start( getTimeStamp() ),
         mutexAddr( mutAddr ),
         client( ClientManager::Get( VDBG_GET_THREAD_ID() ) )
   {
   }
   ~LockWaitGuard()
   {
      end = getTimeStamp();
      ClientManager::EndLockWait( client, mutexAddr, start, end );
   }

   TimeStamp start, end;
   void* mutexAddr;
   Client* client;
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
#include <vector>

namespace vdbg
{
namespace details
{

// ------ SharedMemory.cpp------------
bool SharedMemory::create( const char* path, size_t requestedSize, bool isProducer )
{
   _isProducer = isProducer;
   // Get the size needed for the ringbuf struct
   size_t ringBufSize;
   ringbuf_get_sizes(MAX_THREAD_NB, &ringBufSize, NULL);

   // TODO handle signals
   // signal( SIGINT, sig_callback_handler );
   _path = path;
   _size = requestedSize;

   const int shmFlags = isProducer ? O_CREAT | O_RDWR | O_EXCL : O_RDWR;
   _sharedMemFd = shm_open( path, shmFlags, 0666 );
   if ( _sharedMemFd < 0 )
   {
      if( isProducer )
      {
         printf("ERROR!\nCould not create shared memory segment. You may have not enough free space to"
                " allocate the requested %lu bytes, or might have a previous shared memory segment "
                "that was not cleared properly from a previous run. \n");
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

   _sharedMetaData = (SharedMetaInfo*) sharedMem;
   _ringbuf = (ringbuf_t*) (sharedMem + sizeof( SharedMetaInfo ));
   _data = sharedMem + sizeof( SharedMetaInfo ) + ringBufSize ;
   if( ringbuf_setup( _ringbuf, MAX_THREAD_NB, requestedSize ) < 0 )
   {
      assert( false && "Ring buffer creation failed" );
   }

   // We can only have one consumer
   if( _sharedMetaData->consumerCount.load() > 0 )
   {
      printf("Cannot have more than one instance of the consumer at a time."
             " You might be trying to run the consumer application twice or"
             " have a dangling shared memory segment.\n");
      shm_unlink( path );
      return false;
   }

   // Open semaphore
   const int semaphoreFlags = isProducer ? O_CREAT : 0;
   _semaphore = sem_open( "/mysem", semaphoreFlags, S_IRUSR | S_IWUSR, 1 );

   if( isProducer )
   {
      ++_sharedMetaData->producerCount;
   }
   else
   {
      ++_sharedMetaData->consumerCount;
   }

   return true;
}

int SharedMemory::consumerCount() const VDBG_NOEXCEPT
{
   return sharedMetaInfo()->consumerCount.load();
}

int SharedMemory::producerCount() const VDBG_NOEXCEPT
{
   return sharedMetaInfo()->producerCount.load();
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
      if ( sem_close( _semaphore ) != 0 )
      {
         perror( "Could not close semaphore" );
      }

      if ( sem_unlink("/mysem") < 0 )
      {
         perror( "Could not unlink semaphore" );
      }

      // Decrease producer/consumer
      if( _isProducer )
      {
         --_sharedMetaData->producerCount;
      }
      else
      {
         assert( _sharedMetaData->consumerCount == 1 );
         _sharedMetaData->consumerCount = 0;
      }

      if( _sharedMetaData->producerCount.load() == 0 &&
          _sharedMetaData->consumerCount.load() == 0 )
      {
         printf( "Shared memory cleanup...\n" );
      }

      if ( shm_unlink( _path ) != 0 )
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

SharedMemory ClientManager::sharedMemory;
size_t ClientManager::threadsId[MAX_THREAD_NB] = {0};
std::unique_ptr< Client > ClientManager::clients[MAX_THREAD_NB] = {0};

class Client
{
   struct ShallowTrace
   {
      const char *className, *fctName;
      TimeStamp start, end;
      uint8_t group;
   };

  public:
   Client(size_t id) : _threadId( id )
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
      if( !ClientManager::HasConnectedServer() )
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
      const uint32_t stringDataSize = _nameArrayData.size();
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
         tracesInfo->threadId = (uint32_t)_threadId;
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

   int _pushTraceLevel{0};
   size_t _threadId{0};
   std::vector< ShallowTrace > _shallowTraces;
   std::vector< LockWait > _lockWaits;
   std::vector< const char* > _nameArrayId;
   std::vector< char > _nameArrayData;
   ringbuf_worker_t* _worker{NULL};
   uint32_t _sentStringDataSize{0}; // The size of the string array on the server side
};

void intHandler( int dummy )
{
   printf("CTRL+C\n");
}

Client* ClientManager::Get( size_t threadId, bool createIfMissing /*= true*/ )
{
   if( !ClientManager::sharedMemory.data() )
   {
      signal(SIGINT, intHandler);
      ClientManager::sharedMemory.create( VDBG_SHARED_MEM_NAME, VDBG_SHARED_MEM_SIZE, true );
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
      clients[tIndex] = std::unique_ptr< Client >( new Client(threadId) );

      // Register producer in the ringbuffer
      auto ringBuffer = ClientManager::sharedMemory.ringbuffer();
      ClientManager::clients[tIndex]->_worker = ringbuf_register( ringBuffer, tIndex );
      if ( ClientManager::clients[tIndex]->_worker  == NULL )
      {
         assert( false && "ringbuf_register" );
      } 
   }

   return clients[tIndex].get();
}

void ClientManager::StartProfile( Client* client )
{
   ++client->_pushTraceLevel;
}

void ClientManager::EndProfile(
    Client* client,
    const char* name,
    const char* classStr,
    TimeStamp start,
    TimeStamp end,
    uint8_t group )
{
   const int remainingPushedTraces = --client->_pushTraceLevel;
   client->addProfilingTrace( classStr, name, start, end, group );
   if ( remainingPushedTraces <= 0 )
   {
      client->flushToServer();
   }
}

void ClientManager::EndLockWait( Client* client, void* mutexAddr, TimeStamp start, TimeStamp end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code
   if( client->_pushTraceLevel > 0 )
      client->addWaitLockTrace( mutexAddr, start, end );
}

bool ClientManager::HasConnectedServer() VDBG_NOEXCEPT
{
   return ClientManager::sharedMemory.data() &&
          ClientManager::sharedMemory.consumerCount() > 0;
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
 * Branch prediction macros.
 */
#ifndef __predict_true
#define __predict_true( x ) __builtin_expect( ( x ) != 0, 1 )
#define __predict_false( x ) __builtin_expect( ( x ) != 0, 0 )
#endif

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
      if ( __predict_false( next < written && target >= written ) )
      {
         /* The producer must wait. */
         w->seen_off = RBUF_OFF_MAX;
         return -1;
      }

      if ( __predict_false( target >= rbuf->space ) )
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
   if ( __predict_false( target & WRAP_LOCK_BIT ) )
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
      const ringbuf_off_t end = std::min( rbuf->space, rbuf->end );

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