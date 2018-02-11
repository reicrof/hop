#ifndef HOP_H_
#define HOP_H_

// You can disable completly HOP by setting this variable
// to false
#if !defined( HOP_ENABLED )

// Stubbing all profiling macros so they are disabled
// when HOP_ENABLED is false
#define HOP_PROF_FUNC()
#define HOP_PROF_MEMBER_FUNC()
#define HOP_PROF_FUNC_WITH_GROUP( x )
#define HOP_PROF_MEMBER_FUNC_WITH_GROUP( x )

#else  // We do want to profile


///////////////////////////////////////////////////////////////
/////   THESE ARE THE MACROS YOU SHOULD USE/MODIFY  ///////////
///////////////////////////////////////////////////////////////

#define MAX_THREAD_NB 64
#define HOP_SHARED_MEM_SIZE 32000000

// Create a new profiling trace for a free function
#define HOP_PROF_FUNC() HOP_PROF_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, NULL, __func__, 0 ) )
// Create a new profiling trace for a member function
#define HOP_PROF_MEMBER_FUNC() \
   HOP_PROF_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, typeid( this ).name(), __func__, 0 ) )
// Create a new profiling trace for a free function that falls under category x
#define HOP_PROF_FUNC_WITH_GROUP( x ) HOP_PROF_GUARD_VAR(__LINE__,( __FILE__, __LINE__, NULL, __func__, (x) ) )
// Create a new profiling trace for a member function that falls under category x
#define HOP_PROF_MEMBER_FUNC_WITH_GROUP( x ) \
   HOP_PROF_GUARD_VAR(__LINE__,(( __FILE__, __LINE__, typeid( this ).name(), __func__, x ))

///////////////////////////////////////////////////////////////
/////     EVERYTHING AFTER THIS IS IMPL DETAILS        ////////
///////////////////////////////////////////////////////////////
#include <atomic>
#include <memory>
#include <chrono>
#include <stdint.h>
#include <stdio.h>

// ------ platform.h ------------
// This is most things that are potentially non-portable.
#define HOP_CONSTEXPR constexpr
#define HOP_NOEXCEPT noexcept
#define HOP_STATIC_ASSERT static_assert
// On MacOs the max name length seems to be 30...
#define HOP_SHARED_MEM_MAX_NAME_SIZE 30
#define HOP_SHARED_MEM_PREFIX "/hop_"

/* Windows specific macros and defines */
#if defined(_MSC_VER)
#define NOMINMAX
#include <windows.h>
#define sem_handle HANDLE
#define shm_handle HANDLE

#define likely(x)   x
#define unlikely(x) x

inline const char* HOP_GET_PROG_NAME() HOP_NOEXCEPT
{
   static char name[MAX_PATH];
   static DWORD size = GetModuleFileName( NULL, name, MAX_PATH );
   return name;
}

#define HOP_GET_THREAD_ID() (size_t)GetCurrentThreadId()

// Type defined in unistd.h
#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long

#if !defined(_WIN64)
    #error 32 bit not supported
#endif

#endif

/* Unix (Linux & MacOs) specific macros and defines */
#else
#include <pthread.h>
#include <semaphore.h>
#define sem_handle sem_t*
#define shm_handle int

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define HOP_GET_THREAD_ID() (size_t)pthread_self()

extern char* __progname;
inline const char* HOP_GET_PROG_NAME() HOP_NOEXCEPT
{
   return __progname;
}

#endif

// -----------------------------

// Forward declarations of type used by ringbuffer as adapted from
// Mindaugas Rasiukevicius. See below for Copyright/Disclaimer
typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;

// ------ message.h ------------
namespace hop
{

using Clock = std::chrono::steady_clock;
using Precision = std::chrono::nanoseconds;
inline decltype( std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count() ) getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

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

HOP_CONSTEXPR uint32_t EXPECTED_MSG_INFO_SIZE = 16;
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
HOP_STATIC_ASSERT( sizeof(MsgInfo) == EXPECTED_MSG_INFO_SIZE, "MsgInfo layout has changed unexpectedly" );


using TStrIdx_t = uint32_t;
using TLineNb_t = uint32_t;
using TGroup_t = uint16_t;
using TDepth_t = uint16_t;
HOP_CONSTEXPR uint32_t EXPECTED_TRACE_SIZE = 64;
struct Trace
{
   TimeStamp start, end;   // Timestamp for start/end of this trace
   TStrIdx_t fileNameIdx;  // Index into string array for the file name
   TStrIdx_t classNameIdx; // Index into string array for the class name
   TStrIdx_t fctNameIdx;   // Index into string array for the function name
   TLineNb_t lineNumber;   // Line at which the trace was inserted
   TGroup_t group;         // Group to which this trace belongs
   TDepth_t depth;         // The depth in the callstack of this trace
   char padding[24];
};
HOP_STATIC_ASSERT( sizeof(Trace) == EXPECTED_TRACE_SIZE, "Trace layout has changed unexpectedly" );

HOP_CONSTEXPR uint32_t EXPECTED_LOCK_WAIT_SIZE = 32;
struct LockWait
{
   void* mutexAddress;
   TimeStamp start, end;
   uint32_t padding;
};
HOP_STATIC_ASSERT( sizeof(LockWait) == EXPECTED_LOCK_WAIT_SIZE, "Lock wait layout has changed unexpectedly" );

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
         CONNECTED_PRODUCER = 1 << 0,
         CONNECTED_CONSUMER = 1 << 1,
         LISTENING_CONSUMER = 1 << 2,
      };
      std::atomic< uint32_t > flags{0};
   };

   bool hasConnectedProducer() const HOP_NOEXCEPT;
   void setConnectedProducer( bool ) HOP_NOEXCEPT;
   bool hasConnectedConsumer() const HOP_NOEXCEPT;
   void setConnectedConsumer( bool ) HOP_NOEXCEPT;
   bool hasListeningConsumer() const HOP_NOEXCEPT;
   void setListeningConsumer( bool ) HOP_NOEXCEPT;
   ringbuf_t* ringbuffer() const HOP_NOEXCEPT;
   uint8_t* data() const HOP_NOEXCEPT;
   sem_handle semaphore() const HOP_NOEXCEPT;
   void waitSemaphore() const HOP_NOEXCEPT;
   void signalSemaphore() const HOP_NOEXCEPT;
   const SharedMetaInfo* sharedMetaInfo() const HOP_NOEXCEPT;
   ~SharedMemory();

  private:
   // Pointer into the shared memory
   SharedMetaInfo* _sharedMetaData{NULL};
   ringbuf_t* _ringbuf{NULL};
   uint8_t* _data{NULL};
   // ----------------
   sem_handle _semaphore{NULL};
   bool _isConsumer;
   size_t _size{0};
   shm_handle _sharedMemHandle{};
   char _sharedMemPath[HOP_SHARED_MEM_MAX_NAME_SIZE];
};
// ------ end of SharedMemory.h ------------

class Client;
class ClientManager
{
  public:
   static Client* Get();
   static void StartProfile();
   static void EndProfile(
       const char* fileName,
       const char* fctName,
       const char* className,
       TimeStamp start,
       TimeStamp end,
       TLineNb_t lineNb,
       TGroup_t group );
   static void EndLockWait(
      void* mutexAddr,
      TimeStamp start,
      TimeStamp end );
   static bool HasConnectedConsumer() HOP_NOEXCEPT;
   static bool HasListeningConsumer() HOP_NOEXCEPT;

   static SharedMemory sharedMemory;
};

class ProfGuard
{
  public:
   ProfGuard( const char* fileName, TLineNb_t lineNb, const char* className, const char* fctName, TGroup_t groupId ) HOP_NOEXCEPT
       : _start( getTimeStamp() ),
         _fileName( fileName ),
         _className( className ),
         _fctName( fctName ),
         _lineNb( lineNb ),
         _group( groupId )
   {
      ClientManager::StartProfile();
   }
   ~ProfGuard()
   {
      ClientManager::EndProfile( _fileName, _className, _fctName, _start, getTimeStamp(), _lineNb, _group );
   }

  private:
   TimeStamp _start;
   const char *_fileName, *_className, *_fctName;
   TLineNb_t _lineNb;
   TGroup_t _group;
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

#define HOP_COMBINE( X, Y ) X##Y
#define HOP_PROF_GUARD_VAR( LINE, ARGS ) \
   hop::ProfGuard HOP_COMBINE( hopProfGuard, LINE ) ARGS

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

/* ====================================================================== */

// End of hop declarations

#if defined(HOP_IMPLEMENTATION) || defined(HOP_SERVER_IMPLEMENTATION)

// standard includes
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <vector>

#if !defined( _MSC_VER )

// Unix shared memory includes
#include <fcntl.h> //O_CREAT
#include <cstring> // memcpy
#include <sys/mman.h> // shm_open
#include <unistd.h> // ftruncate

#endif

namespace
{
	static void printErrorMsg(const char* msg)
	{
#if defined( _MSC_VER )
		char err[512];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, 255, NULL);
		printf("%s %s\n", msg, err);
		puts(err);
#else
		perror(msg);
#endif
	}

   sem_handle openSemaphore( const char* name )
   {
      sem_handle sem = NULL;
#if defined( _MSC_VER )
	     sem = CreateSemaphore(NULL, 0, LONG_MAX, name);
#else
         sem = sem_open( name, O_CREAT, S_IRUSR | S_IWUSR, 1 );
#endif

		 if (!sem)
		 {
			 printErrorMsg("Could not open semaphore");
		 }

      return sem;
   }

   void closeSemaphore( sem_handle sem )
   {
 #if defined ( _MSC_VER )
 #else
         if ( sem_close( sem ) != 0 )
         {
            perror( "Could not close semaphore" );
         }
         if ( sem_unlink( "/mysem" ) < 0 )
         {
            perror( "Could not unlink semaphore" );
         }
 #endif
   }

   void* openSharedMemory( const char* path, size_t size, shm_handle* handle )
   {
	   uint8_t* sharedMem = NULL;
#if defined ( _MSC_VER )
	   *handle = CreateFileMapping(
		   INVALID_HANDLE_VALUE,    // use paging file
		   NULL,                    // default security
		   PAGE_READWRITE,          // read/write access
		   0,                       // maximum object size (high-order DWORD)
		   size,                    // maximum object size (low-order DWORD)
		   path);                   // name of mapping object

	   if (*handle == NULL)
	   {
		   return NULL;
	   }
	   sharedMem = (uint8_t*) MapViewOfFile(
		   *handle,
		   FILE_MAP_ALL_ACCESS, // read/write permission
		   0,
		   0,
		   size);

	   if (sharedMem == NULL)
	   {
		   printErrorMsg("Could not map view of file");

		   CloseHandle(*handle);
		   return NULL;
	   }
#else
      *handle = shm_open( path, O_CREAT | O_RDWR, 0666 );
      if ( *handle < 0 )
      {
         return NULL;
      }

      ftruncate( *handle, size );

      sharedMem = (uint8_t*) mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 );
      if ( sharedMem == NULL )
      {
		  printErrorMsg( "Could not map shared memory" );
      }
#endif
      return sharedMem;
   }

   void closeSharedMemory( const char* name, shm_handle handle )
   {
      #if defined( _MSC_VER )
      #else
         if ( shm_unlink( name ) != 0 ) perror( "Could not unlink shared memory" );
      #endif
   }

}

namespace hop
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
   strncpy( _sharedMemPath, path, HOP_SHARED_MEM_MAX_NAME_SIZE - 1 );
   _size = requestedSize;

   // First try to open semaphore
   _semaphore = openSemaphore( "/mysem" );
   if( _semaphore == NULL ) return false;

   // Create the shared memory next
   const size_t totalSize = ringBufSize + requestedSize + sizeof( SharedMetaInfo );
   uint8_t* sharedMem = (uint8_t*)openSharedMemory( path, totalSize, &_sharedMemHandle );
   if( !sharedMem )
   {
      assert( false );
      return false;
      //if( !isConsumer )
         //perror( "Could not shm_open shared memory" );
   }

   // Get pointers inside the shared memoryu
   _sharedMetaData = (SharedMetaInfo*) sharedMem;
   _ringbuf = (ringbuf_t*) (sharedMem + sizeof( SharedMetaInfo ));
   _data = sharedMem + sizeof( SharedMetaInfo ) + ringBufSize ;

   // If there is neither a consumer nor a producer, clear the shared memory, and create
   // the shared ring buffer
   if ( ( _sharedMetaData->flags &
          ( SharedMetaInfo::CONNECTED_PRODUCER | SharedMetaInfo::CONNECTED_CONSUMER ) ) == 0 )
   {
      memset( _ringbuf, 0, totalSize - sizeof( SharedMetaInfo) );
      if ( ringbuf_setup( _ringbuf, MAX_THREAD_NB, requestedSize ) < 0 )
      {
         assert( false && "Ring buffer creation failed" );
      }
   }

   // We can only have one consumer
   if( isConsumer && hasConnectedConsumer() )
   {
      printf("/!\\ WARNING /!\\ \n"
             "Cannot have more than one instance of the consumer at a time."
             " You might be trying to run the consumer application twice or"
             " have a dangling shared memory segment. hop might be unstable"
             " in this state. You could consider manually removing the shared"
             " memory, or restart your client application.\n\n");
      // Force resetting the listening state as this could cause crash. The side
      // effect would simply be that other consumer would stop listening. Not a
      // big deal as there should not be any other consumer...
      _sharedMetaData->flags &= ~(SharedMetaInfo::LISTENING_CONSUMER);
   }

   _sharedMetaData->flags |=
       isConsumer ? SharedMetaInfo::CONNECTED_CONSUMER : SharedMetaInfo::CONNECTED_PRODUCER;

   return true;
}

bool SharedMemory::hasConnectedProducer() const HOP_NOEXCEPT
{
   return (sharedMetaInfo()->flags & SharedMetaInfo::CONNECTED_PRODUCER) > 0;
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
   return (sharedMetaInfo()->flags & SharedMetaInfo::CONNECTED_CONSUMER) > 0;
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
   return (sharedMetaInfo()->flags.load() & mask) == mask;
}

void SharedMemory::setListeningConsumer( bool listening ) HOP_NOEXCEPT
{
   if(listening)
      _sharedMetaData->flags |= SharedMetaInfo::LISTENING_CONSUMER;
   else
      _sharedMetaData->flags &= ~(SharedMetaInfo::LISTENING_CONSUMER);
}

uint8_t* SharedMemory::data() const HOP_NOEXCEPT
{
   return _data;
}

ringbuf_t* SharedMemory::ringbuffer() const HOP_NOEXCEPT
{
   return _ringbuf;
}

sem_handle SharedMemory::semaphore() const HOP_NOEXCEPT
{
   return _semaphore;
}

void SharedMemory::waitSemaphore() const HOP_NOEXCEPT
{
    #if defined(_MSC_VER)
    #else
        sem_wait( _semaphore );
    #endif
}

void SharedMemory::signalSemaphore() const HOP_NOEXCEPT
{
    #if defined(_MSC_VER)
    #else
        sem_post( _semaphore );
    #endif
}

const SharedMemory::SharedMetaInfo* SharedMemory::sharedMetaInfo() const HOP_NOEXCEPT
{
   return _sharedMetaData;
}

void SharedMemory::destroy()
{
   if ( data() )
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
      if ( _sharedMemPath &&
           ( _sharedMetaData->flags &
             ( SharedMetaInfo::CONNECTED_PRODUCER | SharedMetaInfo::CONNECTED_CONSUMER ) ) == 0 )
      {
         printf("Cleaning up shared resources...\n");
         closeSemaphore( _semaphore );
         closeSharedMemory( _sharedMemPath, _sharedMemHandle );
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
#ifndef HOP_SERVER_IMPLEMENTATION

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
      const char *fileName, *className, *fctName;
      TimeStamp start, end;
      TLineNb_t lineNumber;
      TGroup_t group;
      TDepth_t depth;
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
       const char* fileName,
       const char* className,
       const char* fctName,
       TimeStamp start,
       TimeStamp end,
       TLineNb_t lineNb,
       TGroup_t group )
   {
      _shallowTraces.push_back( ShallowTrace{ fileName, className, fctName, start, end, lineNb, group, (TDepth_t)tl_traceLevel } );
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
         const TStrIdx_t newEntryPos = (TStrIdx_t) _stringData.size();
         stringIndex = newEntryPos;
         _stringData.resize( newEntryPos + strlen( strId ) + 1 );
         strcpy( &_stringData[newEntryPos], strId );
         return newEntryPos;
      }

      return stringIndex;
   }


   bool sendTraces()
   {
      // Convert string pointers to index in the string database
      struct StringsIdx
      {
         StringsIdx( TStrIdx_t f, TStrIdx_t c, TStrIdx_t fct )
             : fileName( f ), className( c ), fctName( fct ) {}
         TStrIdx_t fileName, className, fctName;
      };

      std::vector< StringsIdx > classFctNamesIdx;
      classFctNamesIdx.reserve( _shallowTraces.size() );
      for( const auto& t : _shallowTraces  )
      {
         classFctNamesIdx.emplace_back(
             findOrAddStringToDb( t.fileName ),
             findOrAddStringToDb( t.className ),
             findOrAddStringToDb( t.fctName ) );
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
         return false;
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
            t.fileNameIdx = classFctNamesIdx[i].fileName;
            t.classNameIdx = classFctNamesIdx[i].className;
            t.fctNameIdx = classFctNamesIdx[i].fctName;
            t.lineNumber = _shallowTraces[i].lineNumber;
            t.group = _shallowTraces[i].group;
            t.depth = _shallowTraces[i].depth;
         }
      }

      ringbuf_produce( ringbuf, _worker );
      ClientManager::sharedMemory.signalSemaphore();

      // Update sent array size
      _sentStringDataSize = stringDataSize;
      // Free the buffers
      _shallowTraces.clear();

      return true;
   }

   bool sendLockWaits()
   {
      if( _lockWaits.empty() ) return false;

      const size_t lockMsgSize = sizeof( MsgInfo ) + _lockWaits.size() * sizeof( LockWait );

      // Allocate big enough buffer from the shared memory
      ringbuf_t* ringbuf = ClientManager::sharedMemory.ringbuffer();
      const auto offset = ringbuf_acquire( ringbuf, _worker, lockMsgSize );
      if ( offset == -1 )
      {
         printf("Failed to acquire enough shared memory. Consider increasing shared memory size\n");
         _lockWaits.clear();
         return false;
      }

      uint8_t* bufferPtr = &ClientManager::sharedMemory.data()[offset];

      // Fill the buffer with the lock message
      {
         MsgInfo* lwInfo = (MsgInfo*)bufferPtr;
         lwInfo->type = MsgType::PROFILER_WAIT_LOCK;
         lwInfo->threadId = (uint32_t)tl_threadId;
         lwInfo->lockwaits.count = (uint32_t)_lockWaits.size();
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      }

      ringbuf_produce( ringbuf, _worker );
      ClientManager::sharedMemory.signalSemaphore();

      _lockWaits.clear();

      return true;
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

      sendTraces();
      sendLockWaits();
   }

   std::vector< ShallowTrace > _shallowTraces;
   std::vector< LockWait > _lockWaits;
   std::unordered_map< const char*, TStrIdx_t > _stringIndex;
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
      char path[HOP_SHARED_MEM_MAX_NAME_SIZE] = {};
      strncpy( path, HOP_SHARED_MEM_PREFIX, sizeof( HOP_SHARED_MEM_PREFIX ) );
      strncat(
          path, HOP_GET_PROG_NAME(), HOP_SHARED_MEM_MAX_NAME_SIZE - sizeof( HOP_SHARED_MEM_PREFIX ) - 1 );
      bool sucess = ClientManager::sharedMemory.create( path, HOP_SHARED_MEM_SIZE, false );
      assert( sucess && "Could not create shared memory" );
   }

   static int threadCount = 0;
   tl_threadId = HOP_GET_THREAD_ID();
   threadClient.reset( new Client() );

   // Register producer in the ringbuffer
   auto ringBuffer = ClientManager::sharedMemory.ringbuffer();
   threadClient->_worker = ringbuf_register( ringBuffer, threadCount );
   if ( threadClient->_worker  == NULL )
   {
      assert( false && "ringbuf_register" );
   }

   ++threadCount;
   assert( threadCount <= MAX_THREAD_NB );

   return threadClient.get();
}

void ClientManager::StartProfile()
{
   ++tl_traceLevel;
}

void ClientManager::EndProfile(
    const char* fileName,
    const char* className,
    const char* fctName,
    TimeStamp start,
    TimeStamp end,
    TLineNb_t lineNb,
    TGroup_t group )
{
   const int remainingPushedTraces = --tl_traceLevel;
   Client* client = ClientManager::Get();
   if( end - start > 50 ) // Minimum trace time is 50 ns
   {
      client->addProfilingTrace( fileName, className, fctName, start, end, lineNb, group );
   }
   if ( remainingPushedTraces <= 0 )
   {
      client->flushToConsumer();
   }
}

void ClientManager::EndLockWait( void* mutexAddr, TimeStamp start, TimeStamp end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code and if it has a wait time greater than 500ns
   if( end - start > 500 && tl_traceLevel > 0 )
   {
      ClientManager::Get()->addWaitLockTrace( mutexAddr, start, end );
   }
}

bool ClientManager::HasConnectedConsumer() HOP_NOEXCEPT
{
   return ClientManager::sharedMemory.data() &&
          ClientManager::sharedMemory.hasConnectedConsumer();
}

bool ClientManager::HasListeningConsumer() HOP_NOEXCEPT
{
   return ClientManager::sharedMemory.data() &&
          ClientManager::sharedMemory.hasListeningConsumer();
}

#endif  // end !HOP_SERVER_IMPLEMENTATION

} // end of namespace hop


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
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
//    auto client = hop::details::ClientProfiler::Get( threadId, false );
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
//       auto client = reinterpret_cast<hop::details::ClientProfiler::Impl*>( clientProfiler );
//       --client->_pushTraceLevel;
//       client->addWaitLockTrace( mutexAddr, timeStampStart, timeStampEnd );
//       printf( "decrease push trace lvl\n");
//    }
// }

#endif  // end HOP_IMPLEMENTATION

#endif  // !defined(HOP_ENABLED)

#endif  // HOP_H_