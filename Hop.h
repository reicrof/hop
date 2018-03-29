#ifndef HOP_H_
#define HOP_H_

// You can disable completly HOP by setting this variable
// to false
#if !defined( HOP_ENABLED )

// Stubbing all profiling macros so they are disabled
// when HOP_ENABLED is false
#define HOP_PROF( x )
#define HOP_PROF_FUNC()
#define HOP_PROF_GL_FINISH( x )
#define HOP_PROF_FUNC_GL_FINISH()
#define HOP_PROF_FUNC_WITH_GROUP( x )
#define HOP_PROF_MUTEX_LOCK( x )
#define HOP_PROF_MUTEX_UNLOCK( x )

#else  // We do want to profile

///////////////////////////////////////////////////////////////
/////       THESE ARE THE MACROS YOU CAN MODIFY     ///////////
///////////////////////////////////////////////////////////////

#define HOP_MAX_THREAD_NB 64
#define HOP_SHARED_MEM_SIZE 32000000

///////////////////////////////////////////////////////////////
/////       THESE ARE THE MACROS YOU SHOULD USE     ///////////
///////////////////////////////////////////////////////////////

// Create a new profiling trace with specified name
#define HOP_PROF( x ) HOP_PROF_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, (x), 0 ) )

// Create a new profiling trace for a free function
#define HOP_PROF_FUNC() HOP_PROF_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, __func__, 0 ) )

// Create a new profiling trace that will call glFinish() before being destroyed
#define HOP_PROF_GL_FINISH( x ) HOP_PROF_GL_FINISH_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, (x), 0 ) )

// Create a new profiling trace for a free function
#define HOP_PROF_FUNC_GL_FINISH() HOP_PROF_GL_FINISH_GUARD_VAR( __LINE__, ( __FILE__, __LINE__, __func__, 0 ) )

// Create a new profiling trace for a free function that falls under category x
#define HOP_PROF_FUNC_WITH_GROUP( x ) HOP_PROF_GUARD_VAR(__LINE__,( __FILE__, __LINE__, __func__, (x) ) )

// Create a trace that represent the time waiting for a mutex. You need to provide
// a pointer to the mutex that is being locked
#define HOP_PROF_MUTEX_LOCK( x ) HOP_MUTEX_LOCK_GUARD_VAR( __LINE__,( x ) )

// Create an event that correspond to the unlock of the specified mutex. This is
// used to provide stall region. You should provide a pointer to the mutex that
// is being unlocked.
#define HOP_PROF_MUTEX_UNLOCK( x ) HOP_MUTEX_UNLOCK_EVENT( x )

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
   static char fullname[MAX_PATH];
   static char* shortname;
   static bool first = true;
   if (first)
   {
      DWORD size = GetModuleFileName(NULL, fullname, MAX_PATH);
      while (size > 0 && fullname[size] != '\\')
         --size;
      shortname = &fullname[size + 1];
      first = false;
   }
   return shortname;
}

#define HOP_GET_THREAD_ID() (size_t)GetCurrentThreadId()
#define HOP_SLEEP_MS( x ) Sleep( x )

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
#define HOP_SLEEP_MS( x ) usleep( x * 1000 )

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
using TimeDuration = int64_t;

enum class MsgType : uint32_t
{
   PROFILER_TRACE,
   PROFILER_WAIT_LOCK,
   PROFILER_UNLOCK_EVENT,
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

struct UnlockEventsMsgInfo
{
   uint32_t count;
};

HOP_CONSTEXPR uint32_t EXPECTED_MSG_INFO_SIZE = 32;
struct MsgInfo
{
   MsgType type;
   // Thread id from which the msg was sent
   uint32_t threadIndex;
   uint64_t threadId;
   // Specific message data
   union {
      TracesMsgInfo traces;
      LockWaitsMsgInfo lockwaits;
      UnlockEventsMsgInfo unlockEvents;
   };
   unsigned char padding[8];
};
HOP_STATIC_ASSERT( sizeof(MsgInfo) == EXPECTED_MSG_INFO_SIZE, "MsgInfo layout has changed unexpectedly" );


using TStrPtr_t = uint64_t;
using TLineNb_t = uint32_t;
using TGroup_t = uint16_t;
using TDepth_t = uint16_t;
HOP_CONSTEXPR uint32_t EXPECTED_TRACE_SIZE = 40;
struct Trace
{
   TimeStamp start, end;   // Timestamp for start/end of this trace
   TStrPtr_t fileNameId;  // Index into string array for the file name
   TStrPtr_t fctNameId;   // Index into string array for the function name
   TLineNb_t lineNumber;   // Line at which the trace was inserted
   TGroup_t group;         // Group to which this trace belongs
   TDepth_t depth;         // The depth in the callstack of this trace
};
HOP_STATIC_ASSERT( sizeof(Trace) == EXPECTED_TRACE_SIZE, "Trace layout has changed unexpectedly" );

HOP_CONSTEXPR uint32_t EXPECTED_LOCK_WAIT_SIZE = 32;
struct LockWait
{
   void* mutexAddress;
   TimeStamp start, end;
   TDepth_t depth;
   uint16_t padding;
};
HOP_STATIC_ASSERT( sizeof(LockWait) == EXPECTED_LOCK_WAIT_SIZE, "Lock wait layout has changed unexpectedly" );

HOP_CONSTEXPR uint32_t EXPECTED_UNLOCK_EVENT_SIZE = 16;
struct UnlockEvent
{
   void* mutexAddress;
   TimeStamp time;
};
HOP_STATIC_ASSERT( sizeof(UnlockEvent) == EXPECTED_UNLOCK_EVENT_SIZE, "Unlock Event layout has changed unexpectedly" );

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
         USE_GL_FINISH      = 1 << 3,
         RESEND_STRING_DATA = 1 << 4,
      };
      std::atomic< uint32_t > flags{0};
      const size_t requestedSize{0};
   };

   bool hasConnectedProducer() const HOP_NOEXCEPT;
   void setConnectedProducer( bool ) HOP_NOEXCEPT;
   bool hasConnectedConsumer() const HOP_NOEXCEPT;
   void setConnectedConsumer( bool ) HOP_NOEXCEPT;
   bool hasListeningConsumer() const HOP_NOEXCEPT;
   void setListeningConsumer( bool ) HOP_NOEXCEPT;
   bool isUsingGlFinish() const HOP_NOEXCEPT;
   void setUseGlFinish( bool ) HOP_NOEXCEPT;
   bool shouldResendStringData() const HOP_NOEXCEPT;
   void resetResendStringDataFlag() HOP_NOEXCEPT;
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
   shm_handle _sharedMemHandle{};
   char _sharedMemPath[HOP_SHARED_MEM_MAX_NAME_SIZE];
};
// ------ end of SharedMemory.h ------------

class Client;
class ClientManager
{
  public:
   static Client* Get();
   static void CallGlFinish();
   static void StartProfile();
   static void StartProfileGlFinish();
   static void EndProfile(
       const char* fileName,
       const char* fctName,
       TimeStamp start,
       TimeStamp end,
       TLineNb_t lineNb,
       TGroup_t group );
   static void EndProfileGlFinish(
       const char* fileName,
       const char* fctName,
       TimeStamp start,
       TLineNb_t lineNb,
       TGroup_t group );
   static void EndLockWait(
      void* mutexAddr,
      TimeStamp start,
      TimeStamp end );
   static void UnlockEvent( void* mutexAddr, TimeStamp time );
   static bool HasConnectedConsumer() HOP_NOEXCEPT;
   static bool HasListeningConsumer() HOP_NOEXCEPT;

   static SharedMemory sharedMemory;
};

class ProfGuard
{
  public:
   ProfGuard( const char* fileName, TLineNb_t lineNb, const char* fctName, TGroup_t groupId ) HOP_NOEXCEPT
       : _start( getTimeStamp() ),
         _fileName( fileName ),
         _fctName( fctName ),
         _lineNb( lineNb ),
         _group( groupId )
   {
      ClientManager::StartProfile();
   }
   ~ProfGuard()
   {
      ClientManager::EndProfile( _fileName, _fctName, _start, getTimeStamp(), _lineNb, _group );
   }

  private:
   TimeStamp _start;
   const char *_fileName, *_fctName;
   TLineNb_t _lineNb;
   TGroup_t _group;
};

class ProfGuardGLFinish
{
  public:
   ProfGuardGLFinish( const char* fileName, TLineNb_t lineNb, const char* fctName, TGroup_t groupId ) HOP_NOEXCEPT
       : _start( getTimeStamp() ),
         _fileName( fileName ),
         _fctName( fctName ),
         _lineNb( lineNb ),
         _group( groupId )
   {
      ClientManager::StartProfileGlFinish();
   }
   ~ProfGuardGLFinish()
   {
      ClientManager::EndProfileGlFinish( _fileName, _fctName, _start, _lineNb, _group );
   }

  private:
   TimeStamp _start;
   const char *_fileName, *_fctName;
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
#define HOP_PROF_GL_FINISH_GUARD_VAR( LINE, ARGS ) \
   hop::ProfGuardGLFinish HOP_COMBINE( hopProfGuard, LINE ) ARGS
#define HOP_MUTEX_LOCK_GUARD_VAR( LINE, ARGS ) \
   hop::LockWaitGuard HOP_COMBINE( hopMutexLock, LINE ) ARGS
#define HOP_MUTEX_UNLOCK_EVENT( x ) \
   hop::ClientManager::UnlockEvent( x, hop::getTimeStamp() );

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
#include <unordered_set>
#include <vector>
#include <mutex>

#if !defined( _MSC_VER )

// Unix shared memory includes
#include <fcntl.h> //O_CREAT
#include <cstring> // memcpy
#include <sys/mman.h> // shm_open
#include <sys/stat.h> // stat
#include <unistd.h> // ftruncate
#include <dlfcn.h> //dlsym

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
         BOOL success = CloseHandle(sem);
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

   void* createSharedMemory(const char* path, size_t size, shm_handle* handle)
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
           printErrorMsg("Could not create file mapping");
           return NULL;
       }
       sharedMem = (uint8_t*)MapViewOfFile(
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
       *handle = shm_open(path, O_CREAT | O_RDWR, 0666);
       if (*handle < 0)
       {
           return NULL;
       }

       ftruncate(*handle, size);

       sharedMem = (uint8_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0);
       if (sharedMem == NULL)
       {
           printErrorMsg("Could not map shared memory");
       }
#endif
       return sharedMem;
   }

   void* openSharedMemory(const char* path, shm_handle* handle, size_t* totalSize)
   {
       uint8_t* sharedMem = NULL;
#if defined ( _MSC_VER )
       *handle = OpenFileMapping(
           FILE_MAP_ALL_ACCESS,   // read/write access
           FALSE,                 // do not inherit the name
           path);               // name of mapping object

       if (*handle == NULL)
       {
           return NULL;
       }

       sharedMem = (uint8_t*)MapViewOfFile(
           *handle,
           FILE_MAP_ALL_ACCESS, // read/write permission
           0,
           0,
           size);

       if (sharedMem == NULL)
       {
           CloseHandle(*handle);
           return NULL;
       }
#else
      *handle = shm_open( path, O_RDWR, 0666 );
      if ( *handle < 0 )
      {
         printErrorMsg( "Could open shared mem handle" );
         return NULL;
      }

      struct stat fileStat;
      if(fstat(*handle,&fileStat) < 0)
      {
         printErrorMsg( "Could not retrieve shared mem size" );
         return NULL;
      }

      *totalSize = fileStat.st_size;
      ftruncate( *handle, fileStat.st_size );

      sharedMem = (uint8_t*) mmap( NULL, fileStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 );
      if ( sharedMem == NULL )
      {
         printErrorMsg( "Could not map shared memory" );
      }
#endif
      return sharedMem;
   }

   void closeSharedMemory( const char* name, shm_handle handle, void* dataPtr )
   {
#if defined( _MSC_VER )
      UnmapViewOfFile( dataPtr );
      CloseHandle( handle );
#else
      (void)handle;  // Remove unuesed warning
      (void)dataPtr;
      if ( shm_unlink( name ) != 0 ) perror( "Could not unlink shared memory" );
#endif
   }

   void* loadSymbol( const char* libraryName, const char* symbolName )
   {
      void* symbol = NULL;
#if defined( _MSC_VER )
      HMODULE module = LoadLibraryA( libraryName );
      symbol = (void *)GetProcAddress( module, symbolName );
#elif __APPLE__
      (void)libraryName;  // Remove unused
      char symbolNamePrefixed[256];
      strcpy( symbolNamePrefixed + 1, symbolName );
      symbolNamePrefixed[0] = '_';
      symbol = dlsym( RTLD_DEFAULT, symbolName );
#else // Linux case
      void* lib = dlopen( libraryName, RTLD_LAZY );
      symbol = dlsym( lib, symbolName );
#endif

      return symbol;
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
   ringbuf_get_sizes(HOP_MAX_THREAD_NB, &ringBufSize, NULL);

   // TODO handle signals
   // signal( SIGINT, sig_callback_handler );
   strncpy( _sharedMemPath, path, HOP_SHARED_MEM_MAX_NAME_SIZE - 1 );

   // First try to open semaphore
   _semaphore = openSemaphore( "/mysem" );
   if( _semaphore == NULL ) return false;

   // Create or open the shared memory next
   uint8_t* sharedMem = NULL;
   size_t totalSize = 0;
   if( isConsumer )
   {
      sharedMem = (uint8_t*)openSharedMemory(path, &_sharedMemHandle, &totalSize);
   }
   else
   {
      totalSize = ringBufSize + requestedSize + sizeof( SharedMetaInfo );
      sharedMem = (uint8_t*)createSharedMemory(path, totalSize, &_sharedMemHandle);
   }


   if( !sharedMem )
   {
      if( !isConsumer )
          printErrorMsg( "Could not shm_open shared memory" );

      return false;
   }

   // Take a local copy as we do not want to expose the ring buffer before it is
   // actually initialized
   ringbuf_t* localRingBuf = (ringbuf_t*) (sharedMem + sizeof( SharedMetaInfo ));

   // If we are the first producer, we create the shared memory
   if (!isConsumer)
   {
       static bool memoryCreated = false;
       static std::mutex m;
       std::lock_guard< std::mutex > g(m);
       if (!memoryCreated)
       {
           // Set the size of the shared memory in the meta data info. 
           SharedMetaInfo* metaInfo = (SharedMetaInfo*) sharedMem;
           size_t* shmReqSize = const_cast< size_t* >( &metaInfo->requestedSize );
           *shmReqSize = HOP_SHARED_MEM_SIZE;

           // Then setup the ring buffer
           memset(localRingBuf, 0, totalSize - sizeof(SharedMetaInfo));
           if (ringbuf_setup(localRingBuf, HOP_MAX_THREAD_NB, requestedSize) < 0)
           {
               assert(false && "Ring buffer creation failed");
           }
           else
           {
               memoryCreated = true;
           }
       }
   }

   // Get pointers inside the shared memory once it has been initialized
   _sharedMetaData = (SharedMetaInfo*) sharedMem;
   _ringbuf = (ringbuf_t*) (sharedMem + sizeof( SharedMetaInfo ));
   _data = sharedMem + sizeof( SharedMetaInfo ) + ringBufSize ;

   // We can only have one consumer
   if( isConsumer && hasConnectedConsumer() )
   {
      printf("/!\\ WARNING /!\\ \n"
             "Cannot have more than one instance of the consumer at a time."
             " You might be trying to run the consumer application twice or"
             " have a dangling shared memory segment. hop might be unstable"
             " in this state. You could consider manually removing the shared"
             " memory, or restart this excutable cleanly.\n\n");
      // Force resetting the listening state as this could cause crash. The side
      // effect would simply be that other consumer would stop listening. Not a
      // big deal as there should not be any other consumer...
      _sharedMetaData->flags &= ~(SharedMetaInfo::LISTENING_CONSUMER);
      _sharedMetaData->flags |= SharedMetaInfo::RESEND_STRING_DATA;
   }

   if( isConsumer )
      setConnectedConsumer( true );
   else
      setConnectedProducer( true );

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

bool SharedMemory::isUsingGlFinish() const HOP_NOEXCEPT
{
   return _sharedMetaData && (_sharedMetaData->flags & SharedMetaInfo::USE_GL_FINISH) > 0;
}

void SharedMemory::setUseGlFinish( bool useGlFinish ) HOP_NOEXCEPT
{
   if ( useGlFinish )
      _sharedMetaData->flags |= SharedMetaInfo::USE_GL_FINISH;
   else
      _sharedMetaData->flags &= ~SharedMetaInfo::USE_GL_FINISH;
}

bool SharedMemory::shouldResendStringData() const HOP_NOEXCEPT
{
   return _sharedMetaData && (_sharedMetaData->flags & SharedMetaInfo::RESEND_STRING_DATA) > 0;
}

void SharedMemory::resetResendStringDataFlag() HOP_NOEXCEPT
{
   _sharedMetaData->flags &= ~SharedMetaInfo::RESEND_STRING_DATA;
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
    WaitForSingleObject(_semaphore, INFINITE);
#else
    sem_wait( _semaphore );
#endif
}

void SharedMemory::signalSemaphore() const HOP_NOEXCEPT
{
#if defined(_MSC_VER)
        ReleaseSemaphore(_semaphore, 1, NULL);
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
      if ( ( _sharedMetaData->flags &
             ( SharedMetaInfo::CONNECTED_PRODUCER | SharedMetaInfo::CONNECTED_CONSUMER ) ) == 0 )
      {
         printf("Cleaning up shared resources...\n");
         closeSemaphore( _semaphore );
         closeSharedMemory( _sharedMemPath, _sharedMemHandle, _sharedMetaData );
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
thread_local uint32_t tl_threadIndex = 0;
thread_local uint64_t tl_threadId = 0;

class Client
{
  public:
   Client()
   {
      _traces.reserve( 256 );
      _lockWaits.reserve( 64 );
      _unlockEvents.reserve( 64 );
      _stringPtr.reserve( 256 );
      _stringData.reserve( 256 * 32 );

      // Push back first name as empty string
      _stringPtr.insert( 0 );
      for( size_t i = 0; i < sizeof( TStrPtr_t ); ++i )
         _stringData.push_back('\0');
   }

   void addProfilingTrace(
       const char* fileName,
       const char* fctName,
       TimeStamp start,
       TimeStamp end,
       TLineNb_t lineNb,
       TGroup_t group )
   {
      _traces.push_back( Trace{ start, end, (TStrPtr_t)fileName, (TStrPtr_t)fctName, lineNb, group, (TDepth_t)tl_traceLevel } );
   }

   void addWaitLockTrace( void* mutexAddr, TimeStamp start, TimeStamp end, TDepth_t depth )
   {
      _lockWaits.push_back( LockWait{ mutexAddr, start, end, depth } );
   }

   void addUnlockEvent( void* mutexAddr, TimeStamp time )
   {
      _unlockEvents.push_back( UnlockEvent{ mutexAddr, time } );
   }

   bool addStringToDb( const char* strId )
   {
      // Early return on NULL. The db should always contains NULL as first
      // entry
      if( strId == NULL ) return 0;

      auto res = _stringPtr.insert( (TStrPtr_t) strId );
      // If the string was inserted (meaning it was not already there),
      // add it to the database, otherwise do nothing
      if( res.second )
      {
         const size_t newEntryPos = _stringData.size();
         _stringData.resize( newEntryPos + sizeof( TStrPtr_t ) + strlen( strId ) + 1 );
         TStrPtr_t* strIdPtr = (TStrPtr_t*)&_stringData[newEntryPos];
         *strIdPtr = (TStrPtr_t)strId;
         strcpy( &_stringData[newEntryPos + sizeof( TStrPtr_t ) ], strId );
      }

      return res.second;
   }


   bool sendTraces()
   {
      // Add all strings to the database
      for( const auto& t : _traces  )
      {
         addStringToDb( (const char*) t.fileNameId );
         addStringToDb( (const char*) t.fctNameId );
      }

      // Reset sent string size if we are requested to re-send them
      if( ClientManager::sharedMemory.shouldResendStringData() )
      {
         _sentStringDataSize = 0;
         ClientManager::sharedMemory.resetResendStringDataFlag();
      }

      // 1- Get size of profiling traces message
      const uint32_t stringDataSize = _stringData.size();
      assert( stringDataSize >= _sentStringDataSize );
      const uint32_t stringToSendSize = stringDataSize - _sentStringDataSize;
      const size_t profilerMsgSize =
          sizeof( MsgInfo ) + stringToSendSize + sizeof( Trace ) * _traces.size();

      // Allocate big enough buffer from the shared memory
      ringbuf_t* ringbuf = ClientManager::sharedMemory.ringbuffer();
      const bool messageWayToBig = profilerMsgSize > HOP_SHARED_MEM_SIZE;
      ssize_t offset = -1;
      if( !messageWayToBig )
      {
         offset = ringbuf_acquire( ringbuf, _worker, profilerMsgSize );
      }

       if ( offset == -1 )
       {
          printf("Failed to acquire enough shared memory. Consider increasing shared"
                 " memory size if you see this message more than once\n");
          _traces.clear();
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
         Trace* traceToSend = (Trace*)( bufferPtr + sizeof( MsgInfo ) + stringToSendSize );

         tracesInfo->type = MsgType::PROFILER_TRACE;
         tracesInfo->threadId = tl_threadId;
         tracesInfo->threadIndex = tl_threadIndex;
         tracesInfo->traces.stringDataSize = stringToSendSize;
         tracesInfo->traces.traceCount = (uint32_t)_traces.size();

         // Copy string data into its array
         const auto itFrom = _stringData.begin() + _sentStringDataSize;
         std::copy( itFrom, itFrom + stringToSendSize, stringData );

         // Copy trace information into buffer to send
         std::copy( _traces.begin(), _traces.end(), traceToSend );
      }

      ringbuf_produce( ringbuf, _worker );
      ClientManager::sharedMemory.signalSemaphore();

      // Update sent array size
      _sentStringDataSize = stringDataSize;
      // Free the buffers
      _traces.clear();

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
         lwInfo->threadId = tl_threadId;
         lwInfo->threadIndex = tl_threadIndex;
         lwInfo->lockwaits.count = (uint32_t)_lockWaits.size();
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      }

      ringbuf_produce( ringbuf, _worker );
      ClientManager::sharedMemory.signalSemaphore();

      _lockWaits.clear();

      return true;
   }

   bool sendUnlockEvents()
   {
      if( _unlockEvents.empty() ) return false;

      const size_t unlocksMsgSize = sizeof( MsgInfo ) + _unlockEvents.size() * sizeof( UnlockEvent );

      // Allocate big enough buffer from the shared memory
      ringbuf_t* ringbuf = ClientManager::sharedMemory.ringbuffer();
      const auto offset = ringbuf_acquire( ringbuf, _worker, unlocksMsgSize );
      if ( offset == -1 )
      {
         printf("Failed to acquire enough shared memory. Consider increasing shared memory size\n");
         _unlockEvents.clear();
         return false;
      }

      uint8_t* bufferPtr = &ClientManager::sharedMemory.data()[offset];

      // Fill the buffer with the lock message
      {
         MsgInfo* uInfo = (MsgInfo*)bufferPtr;
         uInfo->type = MsgType::PROFILER_UNLOCK_EVENT;
         uInfo->threadId = tl_threadId;
         uInfo->threadIndex = tl_threadIndex;
         uInfo->unlockEvents.count = (uint32_t)_unlockEvents.size();
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _unlockEvents.data(), _unlockEvents.size() * sizeof( UnlockEvent ) );
      }

      ringbuf_produce( ringbuf, _worker );
      ClientManager::sharedMemory.signalSemaphore();

      _unlockEvents.clear();

      return true;
   }

   void flushToConsumer()
   {
      if( !ClientManager::HasListeningConsumer() )
      {
         _traces.clear();
         _lockWaits.clear();
         _unlockEvents.clear();
         // Also reset the string data that was sent since we might
         // have lost the connection with the consumer
         _sentStringDataSize = 0;
         return;
      }

      sendTraces();
      sendLockWaits();
      sendUnlockEvents();
   }

   std::vector< Trace > _traces;
   std::vector< LockWait > _lockWaits;
   std::vector< UnlockEvent > _unlockEvents;
   std::unordered_set< TStrPtr_t > _stringPtr;
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
      (void) sucess; // Removed unused warning
      assert( sucess && "Could not create shared memory" );
   }

   // Static variable that counts the total number of thread
   static std::atomic< int > threadCount{ 0 };
   tl_threadIndex = threadCount.fetch_add(1);
   tl_threadId = HOP_GET_THREAD_ID();

   threadClient.reset( new Client() );

   // Register producer in the ringbuffer
   assert(tl_threadIndex <= HOP_MAX_THREAD_NB);
   auto ringBuffer = ClientManager::sharedMemory.ringbuffer();
   threadClient->_worker = ringbuf_register( ringBuffer, tl_threadIndex);
   if ( threadClient->_worker  == NULL )
   {
      assert( false && "ringbuf_register" );
   }

   return threadClient.get();
}

void ClientManager::CallGlFinish()
{
#ifdef _MSC_VER
   static const char* glLibName = "opengl32.dll";
#else
   static const char* glLibName = "libGL.so";
#endif

   static void (*glFinishPtr)() = NULL;

   // If we request glFinish, load the symbol and call it
   if( ClientManager::sharedMemory.isUsingGlFinish() )
   {
      // Load the symobl
      if( !glFinishPtr )
      {
         glFinishPtr = (void (*)())loadSymbol( glLibName, "glFinish" );
      }

      // Call the symbol
      if( glFinishPtr )
      {
         (*glFinishPtr)();
      }
      else
      {
         printf("Error loading glFinish() symbol! glFinish() was not called!\n");
      }
   }
}

void ClientManager::StartProfile()
{
   ++tl_traceLevel;
}

void ClientManager::StartProfileGlFinish()
{
   ++tl_traceLevel;
   ClientManager::CallGlFinish();
}

void ClientManager::EndProfile(
    const char* fileName,
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
      client->addProfilingTrace( fileName, fctName, start, end, lineNb, group );
   }
   if ( remainingPushedTraces <= 0 )
   {
      client->flushToConsumer();
   }
}

void ClientManager::EndProfileGlFinish(
    const char* fileName,
    const char* fctName,
    TimeStamp start,
    TLineNb_t lineNb,
    TGroup_t group )
{
   ClientManager::CallGlFinish();
   
   const int remainingPushedTraces = --tl_traceLevel;
   Client* client = ClientManager::Get();
   const TimeStamp end = getTimeStamp();
   
   if( end - start > 50 ) // Minimum trace time is 50 ns
   {
      client->addProfilingTrace( fileName, fctName, start, end, lineNb, group );
   }
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
      ClientManager::Get()->addWaitLockTrace( mutexAddr, start, end, tl_traceLevel );
   }
}

void ClientManager::UnlockEvent( void* mutexAddr, TimeStamp time )
{
   if( tl_traceLevel > 0 )
   {
      ClientManager::Get()->addUnlockEvent( mutexAddr, time );
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
   assert( nworkers == HOP_MAX_THREAD_NB );
   if ( ringbuf_size ) *ringbuf_size = offsetof( ringbuf_t, workers[HOP_MAX_THREAD_NB] );
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

#endif  // end HOP_IMPLEMENTATION

#endif  // !defined(HOP_ENABLED)

#endif  // HOP_H_
