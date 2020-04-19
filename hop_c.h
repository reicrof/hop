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
#ifndef HOP_C_H_
#define HOP_C_H_

#ifdef __cplusplus
extern "C"
{
#endif

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

// By default HOP will use a call to RDTSCP to get the current timestamp of the
// CPU. A mismatch in synchronization was noted on some machine having multiple
// physical CPUs. This would show up in the viewer as infinitly long traces or
// traces that overlaps each-other. The std::chrono library seems to handle
// this use case correctly. You can therefore enable the use of std::chrono
// by setting this variable to 1. This will also increase the over-head of
// HOP in your application.
#if !defined( HOP_USE_STD_CHRONO )
#define HOP_USE_STD_CHRONO 0
#endif

///////////////////////////////////////////////////////////////
/////       THESE ARE THE MACROS YOU SHOULD USE     ///////////
///////////////////////////////////////////////////////////////
#if defined( _MSC_VER )
#define HOP_FCT_NAME __FUNCTION__
#else
#define HOP_FCT_NAME __PRETTY_FUNCTION__
#endif

#define HOP_ENTER( x )   hop_enter( __FILE__, __LINE__, (x) )
#define HOP_ENTER_FUNC() hop_enter( __FILE__, __LINE__, HOP_FCT_NAME )
#define HOP_LEAVE()      hop_leave()

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
#define HOP_VERSION 0.91f
#define HOP_ZONE_MAX 255
#define HOP_ZONE_DEFAULT 0

#include <stdint.h> // integer types
#include <stdio.h>  // printf

#if defined( _MSC_VER ) && defined( HOP_IMPLEMENTATION )
#define HOP_API __declspec( dllexport )
#else
#define HOP_API
#endif

/*
#ifdef HOP_USE_STD_CHRONO
   #include <chrono>
#endif
*/

/* Windows specific macros and defines */
#if defined( _MSC_VER )
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <tchar.h>
#include <intrin.h>  // __rdtscp 
typedef void* shm_handle;  // HANDLE is a void*
typedef TCHAR HOP_CHAR;

// Type defined in unistd.h
#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long
#endif                  // _WIN64
#else                   /* Unix (Linux & MacOs) specific macros and defines */
#include <sys/types.h>  // ssize_t
typedef int shm_handle;
typedef char HOP_CHAR;
#endif

// typedef _Thread_local hop_thread_local;
#define hop_thread_local    _Thread_local
#define hop_atomic_uint64   volatile uint64_t
#define hop_atomic_uint32   volatile uint32_t

typedef int      hop_bool_t;
enum { hop_false, hop_true };

typedef uint64_t hop_timestamp_t;
typedef int64_t  hop_timeduration_t;
typedef uint64_t hop_str_ptr_t;
typedef uint32_t hop_linenb_t;
typedef uint32_t hop_core_t;
typedef uint16_t hop_depth_t;
typedef uint16_t hop_zone_t;
typedef void*    hop_mutex_t;

// -----------------------------
// Forward declarations of type used by ringbuffer as adapted from
// Mindaugas Rasiukevicius. See below for Copyright/Disclaimer
typedef struct ringbuf ringbuf_t;
typedef struct ringbuf_worker ringbuf_worker_t;
// -----------------------------

typedef enum hop_msg_type
{
   PROFILER_TRACE,
   PROFILER_STRING_DATA,
   PROFILER_WAIT_LOCK,
   PROFILER_UNLOCK_EVENT,
   PROFILER_HEARTBEAT,
   PROFILER_CORE_EVENT,
   INVALID_MESSAGE
} hop_msg_type;

typedef struct hop_msg_info
{
   hop_msg_type type;
   // Thread id from which the msg was sent
   uint32_t threadIndex;
   uint64_t threadId;
   hop_timestamp_t timeStamp;
   hop_str_ptr_t threadName;
   uint32_t count;
} hop_msg_info;

typedef struct hop_traces
{
   uint32_t count;
   uint32_t maxSize;
   hop_timestamp_t *starts, *ends;  // Timestamp for start/end of this trace
   hop_str_ptr_t* fileNameIds;      // Index into string array for the file name
   hop_str_ptr_t* fctNameIds;       // Index into string array for the function name
   hop_linenb_t* lineNumbers;       // Line at which the trace was inserted
   hop_depth_t* depths;             // The depth in the callstack of this trace
   hop_zone_t* zones;               // Zone to which this trace belongs
} hop_traces;

typedef struct hop_lock_wait
{
   hop_mutex_t mutexAddress;
   hop_timestamp_t start, end;
   hop_depth_t depth;
   uint16_t padding;
} hop_lock_wait;

typedef struct hop_unlock_event
{
   hop_mutex_t mutexAddress;
   hop_timestamp_t time;
} hop_unlock_event;

typedef struct hop_core_event
{
   hop_timestamp_t start, end;
   hop_core_t core;
} hop_core_event;

hop_timestamp_t hop_rdtscp( uint32_t* aux )
{
#if defined( _MSC_VER )
   return __rdtscp( aux );
#else
   uint64_t rax, rdx;
   asm volatile( "rdtscp\n" : "=a"( rax ), "=d"( rdx ), "=c"( *aux ) : : );
   return ( rdx << 32U ) + rax;
#endif
}

hop_timestamp_t hop_get_timestamp( hop_core_t* core )
{
   // We return the timestamp with the first bit set to 0. We do not require this last cycle/nanosec
   // of precision. It will instead be used to flag if a trace uses dynamic strings or not in its
   // start time. See hop::StartProfileDynString
   // #if HOP_USE_STD_CHRONO != 0
   //    using namespace std::chrono;
   //    core = 0;
   //    return (hop_timestamp_t)duration_cast<nanoseconds>( steady_clock::now().time_since_epoch()
   //    ).count() &
   //           ~1ULL;
   // #else
   return hop_rdtscp( core ) & ~1ULL;
   //#endif
}

hop_timestamp_t hop_get_timestamp_no_core()
{
   hop_core_t dummyCore;
   return hop_get_timestamp( &dummyCore );
}

hop_bool_t hop_initialize();
void hop_terminate();

/* End of declarations */

#include <assert.h>
#include <string.h>

#define HOP_MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define HOP_MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define HOP_UNUSED( x ) (void)( x )
#define HOP_MALLOC( x ) malloc( (x) )
#define HOP_REALLOC( ptr, size ) realloc( (ptr), (size) )
#define HOP_FREE( x )   free( (x) )
#define HOP_ASSERT( x ) ( (x) )

#if !defined( _MSC_VER )
#include <unistd.h>    // ftruncate, getpid

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = "/hop_";

#define HOP_STRLEN( str ) strlen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )

#define HOP_LIKELY( x ) __builtin_expect( !!( x ), 1 )
#define HOP_UNLIKELY( x ) __builtin_expect( !!( x ), 0 )

#define HOP_GET_THREAD_ID() reinterpret_cast<size_t>( pthread_self() )

/*
// Unix shared memory includes
#include <fcntl.h>     // O_CREAT
#include <cstring>     // memcpy
#include <pthread.h>   // pthread_self
#include <sys/mman.h>  // shm_open
#include <sys/stat.h>  // stat

#define HOP_SLEEP_MS( x ) usleep( x * 1000 )
*/
int HOP_GET_PID() { return getpid(); }

#else  // !defined( _MSC_VER )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = _T("/hop_");

#define HOP_STRLEN( str ) _tcslen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) _tcsncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) _tcsncat( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat_s( ( dst ), ( count ), ( src ), ( count ) )

#define HOP_LIKELY( x ) ( x )
#define HOP_UNLIKELY( x ) ( x )

#define HOP_GET_THREAD_ID() ( size_t ) GetCurrentThreadId()
/*


#define HOP_SLEEP_MS( x ) Sleep( x )
*/
inline int HOP_GET_PID() { return GetCurrentProcessId(); }

#endif  // !defined( _MSC_VER )



typedef enum hop_connection_state
{
   HOP_NO_TARGET_PROCESS,
   HOP_NOT_CONNECTED,
   HOP_CONNECTED,
   HOP_CONNECTED_NO_CLIENT,
   HOP_PERMISSION_DENIED,
   HOP_INVALID_VERSION,
   HOP_UNKNOWN_CONNECTION_ERROR
} hop_connection_state;

static struct hop_shared_memory* g_sharedMemory;

hop_connection_state
hop_create_shared_memory( int pid, uint64_t requestedSize, hop_bool_t isConsumer );
void hop_destroy_shared_memory();

hop_bool_t hop_initialize()
{
   hop_connection_state state = hop_create_shared_memory( HOP_GET_PID(), HOP_SHARED_MEM_SIZE, hop_false );
   if( state != HOP_CONNECTED )
   {
      const char* reason = "";
      if( state == HOP_PERMISSION_DENIED ) reason = " : Permission Denied";

      printf( "HOP - Could not create shared memory%s. HOP will not be able to run\n", reason );
      return hop_false;
   }
   return hop_true;
}

void hop_terminate()
{
   hop_destroy_shared_memory();
}

/*Viewer declaration*/
#define HOP_SHARED_MEM_MAX_NAME_SIZE 30

typedef enum hop_shared_memory_state_bits
{
   HOP_CONNECTED_PRODUCER = 1 << 0,
   HOP_CONNECTED_CONSUMER = 1 << 1,
   HOP_LISTENING_CONSUMER = 1 << 2
} hop_shared_memory_state_bits;
typedef uint32_t hop_shared_memory_state;

typedef uint64_t	ringbuf_off_t;

struct ringbuf_worker {
   volatile ringbuf_off_t	seen_off;
   int			registered;
};

struct ringbuf {
   /* Ring buffer space. */
   size_t			space;

   /*
    * The NEXT hand is atomically updated by the producer.
    * WRAP_LOCK_BIT is set in case of wrap-around; in such case,
    * the producer can update the 'end' offset.
    */
   volatile ringbuf_off_t	next;
   ringbuf_off_t		end;

   /* The following are updated by the consumer. */
   ringbuf_off_t		written;
   unsigned		nworkers;
   ringbuf_worker_t	workers[];
};

typedef struct hop_ipc_segment
{
   float clientVersion;
   uint32_t maxThreadNb;
   uint64_t requestedSize;
   hop_atomic_uint64 lastResetTimeStamp;
   hop_atomic_uint64 lastHeartbeatTimeStamp;
   hop_atomic_uint32 state;
   hop_bool_t usingStdChronoTimeStamps;

   ringbuf_t ringbuf;
   uint8_t* data;
} hop_ipc_segment;

typedef struct hop_shared_memory
{
   hop_ipc_segment* ipcSegment;

   HOP_CHAR sharedMemPath[HOP_SHARED_MEM_MAX_NAME_SIZE];
   shm_handle sharedMemHandle;
   int pid;
   hop_bool_t isConsumer;
} hop_shared_memory;


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
int ringbuf_setup(ringbuf_t*, unsigned, size_t);
void ringbuf_get_sizes(unsigned, size_t*, size_t*);

ringbuf_worker_t* ringbuf_register(ringbuf_t*, unsigned);
void ringbuf_unregister(ringbuf_t*, ringbuf_worker_t*);

ssize_t ringbuf_acquire(ringbuf_t*, ringbuf_worker_t*, size_t);
void ringbuf_produce(ringbuf_t*, ringbuf_worker_t*);
size_t ringbuf_consume(ringbuf_t*, size_t*);
void ringbuf_release(ringbuf_t*, size_t);

/*
 * Atomic operations and memory barriers.  If C11 API is not available,
 * then wrap the GCC builtin routines.
 *
 * Note: This atomic_compare_exchange_weak does not do the C11 thing of
 * filling *(expected) with the actual value, because we don't need
 * that here.
 */
#ifndef atomic_compare_exchange_weak
#define	atomic_compare_exchange_weak(ptr, expected, desired) \
    __sync_bool_compare_and_swap(ptr, *(expected), desired)
#endif

#ifndef atomic_thread_fence
#define	memory_order_relaxed	__ATOMIC_RELAXED
#define	memory_order_acquire	__ATOMIC_ACQUIRE
#define	memory_order_release	__ATOMIC_RELEASE
#define	memory_order_seq_cst	__ATOMIC_SEQ_CST
#define	atomic_thread_fence(m)	__atomic_thread_fence(m)
#endif
#ifndef atomic_store_explicit
#define	atomic_store_explicit	__atomic_store_n
#endif
#ifndef atomic_load_explicit
#define	atomic_load_explicit	__atomic_load_n
#endif
#ifndef atomic_fetch_add_explicit
#define	atomic_fetch_add_explicit	__atomic_fetch_add
#endif


typedef struct hop_hash_set* hop_hash_set_t;
hop_hash_set_t hop_hash_set_create();
void hop_hash_set_destroy( hop_hash_set_t set );
void hop_hash_set_clear( hop_hash_set_t set );
int hop_hash_set_insert( hop_hash_set_t set, const void* value );

/* -------------------------------------------------------
                  End of declaration
-------------------------------------------------------  */

#include <errno.h>
#include <math.h>

#if !defined( _MSC_VER )
#include <fcntl.h>     // O_CREAT
#include <pthread.h>   // pthread_self
#include <sys/mman.h>  // shm_open
#include <sys/stat.h>  // stat
#endif

/*Viewer implementation*/

static hop_connection_state err_to_connection_state( uint32_t err );
static void* open_ipc_memory(
    const HOP_CHAR* path,
    shm_handle* hdl,
    uint64_t* size,
    hop_connection_state* state );
static void* create_ipc_memory(
    const HOP_CHAR* path,
    uint64_t size,
    shm_handle* handle,
    hop_connection_state* state );
static void close_ipc_memory( const HOP_CHAR* name, shm_handle handle, void* dataPtr );

static uint32_t atomic_set_bit( hop_atomic_uint32* value, uint32_t bitToSet );
static uintptr_t atomic_clear_bit( hop_atomic_uint32* value, uint32_t bitToSet );

static hop_bool_t has_connected_producer( hop_shared_memory* mem );
static void set_connected_producer( hop_shared_memory* mem, hop_bool_t );
static hop_bool_t has_connected_consumer( hop_shared_memory* mem );
static void set_connected_consumer( hop_shared_memory* mem, hop_bool_t );
static hop_bool_t has_listening_consumer( hop_shared_memory* mem );
static void set_listening_consumer( hop_shared_memory* mem, hop_bool_t );
static void set_reset_timestamp( hop_shared_memory* mem, hop_timestamp_t t );

hop_connection_state hop_create_shared_memory( int pid, uint64_t requestedSize, hop_bool_t isConsumer )
{
   HOP_ASSERT( !g_sharedMemory );
   
   g_sharedMemory = (hop_shared_memory*) HOP_MALLOC( sizeof(hop_shared_memory) );

   char pidStr[16];
   snprintf( pidStr, sizeof( pidStr ), "%d", pid );
   // Create shared mem name
   HOP_STRNCPYW(
       g_sharedMemory->sharedMemPath,
       HOP_SHARED_MEM_PREFIX,
       HOP_STRLEN( HOP_SHARED_MEM_PREFIX ) + 1 );
   HOP_STRNCATW(
       g_sharedMemory->sharedMemPath,
       pidStr,
       HOP_SHARED_MEM_MAX_NAME_SIZE - HOP_STRLEN( HOP_SHARED_MEM_PREFIX ) - 1 );

   // Try to open shared memory
   hop_connection_state state  = HOP_CONNECTED;
   uint64_t totalSize          = 0;
   hop_ipc_segment* ipcSegment = (hop_ipc_segment*)open_ipc_memory(
       g_sharedMemory->sharedMemPath, &g_sharedMemory->sharedMemHandle, &totalSize, &state );

   // If we are unable to open the shared memory
   if( !ipcSegment )
   {
      // If we are not a consumer, we need to create it
      if( !isConsumer )
      {
         size_t ringBufSize;
         ringbuf_get_sizes( HOP_MAX_THREAD_NB, &ringBufSize, NULL );
         totalSize  = ringBufSize + requestedSize + sizeof( hop_ipc_segment );
         ipcSegment = (hop_ipc_segment*)create_ipc_memory(
             g_sharedMemory->sharedMemPath, totalSize, &g_sharedMemory->sharedMemHandle, &state );
      }
   }

   // If we still are not able to get the shared memory, return failure state
   if( !ipcSegment )
   {
      hop_destroy_shared_memory();
      return state;
   }

   if( !isConsumer )
   {
      // Set client's info in the shared memory for the viewer to access
      ipcSegment->clientVersion            = HOP_VERSION;
      ipcSegment->maxThreadNb              = HOP_MAX_THREAD_NB;
      ipcSegment->requestedSize            = HOP_SHARED_MEM_SIZE;
      ipcSegment->usingStdChronoTimeStamps = HOP_USE_STD_CHRONO;
      ipcSegment->lastResetTimeStamp       = hop_get_timestamp_no_core();

      // Take a local copy as we do not want to expose the ring buffer before it is
      // actually initialized
      if( ringbuf_setup( &ipcSegment->ringbuf, HOP_MAX_THREAD_NB, requestedSize ) < 0 )
      {
         hop_destroy_shared_memory();
         return HOP_UNKNOWN_CONNECTION_ERROR;
      }
   }
   else  // Check if client has compatible version
   {
      if( fabsf( ipcSegment->clientVersion - HOP_VERSION ) > 0.001f )
      {
         printf(
             "HOP - Client's version (%f) does not match HOP viewer version (%f)\n",
             ipcSegment->clientVersion,
             HOP_VERSION );
         hop_destroy_shared_memory();
         return HOP_INVALID_VERSION;
      }
   }

   // Get the size needed for the ringbuf struct
   size_t ringBufSize;
   ringbuf_get_sizes( ipcSegment->maxThreadNb, &ringBufSize, NULL );

   g_sharedMemory->ipcSegment = ipcSegment;
   g_sharedMemory->pid        = pid;
   g_sharedMemory->isConsumer = isConsumer;

   if( isConsumer )
   {
      set_reset_timestamp( g_sharedMemory, hop_get_timestamp_no_core() );
      // We can only have one consumer
      if( has_connected_consumer( g_sharedMemory ) )
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
         atomic_clear_bit( &g_sharedMemory->ipcSegment->state, HOP_LISTENING_CONSUMER );
      }
   }

   isConsumer ? set_connected_consumer( g_sharedMemory, hop_true ) : set_connected_producer( g_sharedMemory, hop_true );
   return state;
}

void hop_destroy_shared_memory()
{
   if( g_sharedMemory )
   {
      if( g_sharedMemory->isConsumer )
      {
         set_listening_consumer( g_sharedMemory, hop_false );
         set_connected_consumer( g_sharedMemory, hop_false );
      }
      else
      {
         set_connected_producer( g_sharedMemory, hop_false );
      }
      
      if( g_sharedMemory->ipcSegment )
      {
         uint32_t state = atomic_load_explicit( &g_sharedMemory->ipcSegment->state, memory_order_seq_cst );
         if( ( state & ( HOP_CONNECTED_PRODUCER | HOP_CONNECTED_CONSUMER ) ) == 0 )
         {
            printf( "HOP - Cleaning up shared memory...\n" );
            close_ipc_memory(
                g_sharedMemory->sharedMemPath,
                g_sharedMemory->sharedMemHandle,
                g_sharedMemory->ipcSegment );
         }
      }
   }
   HOP_FREE( g_sharedMemory );
}

static uint32_t align_on( uint32_t val, uint32_t alignment )
{
   return ( ( val + alignment - 1 ) & ~( alignment - 1 ) );
}

typedef struct pending_traces_t
{
   uint32_t count;
   uint32_t maxSize;
   hop_timestamp_t *starts, *ends;  // Timestamp for start/end of this trace
   hop_str_ptr_t* fileNameIds;      // Index into string array for the file name
   hop_str_ptr_t* fctNameIds;       // Index into string array for the function name
   hop_linenb_t* lineNumbers;       // Line at which the trace was inserted
   hop_depth_t* depths;             // The depth in the callstack of this trace
   hop_zone_t* zones;               // Zone to which this trace belongs
} pending_traces_t;

typedef struct local_context_t
{
   pending_traces_t pendingTraces;
   uint32_t openTraceIdx;  // Index of the last opened trace
   hop_depth_t traceLevel;
   hop_zone_t zoneId;

   hop_str_ptr_t threadName;
   char threadNameBuffer[64];

   uint64_t threadId;     // ID of the thread as seen by the OS
   uint32_t threadIndex;  // Index of the tread as they are coming in
   ringbuf_worker_t* ringbufWorker;

   hop_hash_set_t stringHashSet;
   uint32_t       stringDataSize;
   uint32_t       stringDataCapacity;
   char*          stringData;
} local_context_t;

static hop_thread_local local_context_t tl_context;

static void alloc_traces( pending_traces_t* t, unsigned size )
{
   t->maxSize     = size;
   t->starts      = (hop_timestamp_t*)HOP_REALLOC( t->starts, size * sizeof( hop_timestamp_t ) );
   t->ends        = (hop_timestamp_t*)HOP_REALLOC( t->ends, size * sizeof( hop_timestamp_t ) );
   t->depths      = (hop_depth_t*)HOP_REALLOC( t->depths, size * sizeof( hop_depth_t ) );
   t->fctNameIds  = (hop_str_ptr_t*)HOP_REALLOC( t->fctNameIds, size * sizeof( hop_str_ptr_t ) );
   t->fileNameIds = (hop_str_ptr_t*)HOP_REALLOC( t->fileNameIds, size * sizeof( hop_str_ptr_t ) );
   t->lineNumbers = (hop_linenb_t*)HOP_REALLOC( t->lineNumbers, size * sizeof( hop_linenb_t ) );
   t->zones       = (hop_zone_t*)HOP_REALLOC( t->zones, size * sizeof( hop_zone_t ) );
}

static void free_traces( pending_traces_t* t )
{
   free( t->starts );
   free( t->ends );
   free( t->depths );
   free( t->fctNameIds );
   free( t->fileNameIds );
   free( t->lineNumbers );
   free( t->zones );
   memset( t, 0, sizeof( pending_traces_t ) );
}

static void copy_pending_traces_to( const pending_traces_t* t, void* outBuffer )
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

static hop_bool_t thread_local_context_valid( local_context_t* ctxt )
{
   return ctxt && ctxt->ringbufWorker;
}

static hop_bool_t thread_local_context_create( local_context_t* ctxt )
{
   static uint32_t g_totalThreadCount;  // Index of the tread as they are coming in
   ctxt->threadIndex = atomic_fetch_add_explicit( &g_totalThreadCount, 1, memory_order_seq_cst );

   if( ctxt->threadIndex > HOP_MAX_THREAD_NB )
   {
      printf( "Maximum number of threads reached. No trace will be available for this thread\n" );
      ctxt->pendingTraces.count = 0;
      return hop_false;
   }

   ctxt->stringData = (char*) HOP_MALLOC( 1024 );
   alloc_traces( &ctxt->pendingTraces, 256 );
   ctxt->stringHashSet = hop_hash_set_create();
   ctxt->threadId = HOP_GET_THREAD_ID();
   ctxt->ringbufWorker =
       ringbuf_register( &g_sharedMemory->ipcSegment->ringbuf, ctxt->threadIndex );

   return ctxt->ringbufWorker != NULL;
}

// C-style string hash inspired by Stackoverflow question
// based on the Java string hash fct. If its good enough
// for java, it should be good enough for me...
static hop_str_ptr_t c_str_hash( const char* str, size_t strLen )
{
   hop_str_ptr_t result      = 0;
   const hop_str_ptr_t prime = 31;
   for( size_t i = 0; i < strLen; ++i )
   {
      result = str[i] + ( result * prime );
   }
   return result;
}

static hop_bool_t add_string_to_db_internal(
    local_context_t* ctxt,
    hop_str_ptr_t strId,  // String ID (hash or actual pointer)
    const char* strPtr,   // Pointer to string data
    uint32_t strLen )     // String length
{
   const hop_bool_t inserted = hop_hash_set_insert( ctxt->stringHashSet, (void*)strId );
   // If the string was inserted to the hash set, also add it to the data section
   if( inserted )
   {
      const size_t newEntryPos = ctxt->stringDataSize;
      HOP_ASSERT( ( newEntryPos & 7 ) == 0 );  // Make sure we are 8 byte aligned
      const size_t alignedStrLen = align_on( strLen + 1, 8 );

      ctxt->stringDataSize += newEntryPos + sizeof( hop_str_ptr_t ) + alignedStrLen;
      if( ctxt->stringDataSize >= ctxt->stringDataCapacity )
      {
         ctxt->stringDataCapacity *= 2;
         HOP_REALLOC( ctxt->stringData, ctxt->stringDataCapacity );
      }

      hop_str_ptr_t* strIdPtr = (hop_str_ptr_t*)( &ctxt->stringData[newEntryPos] );
      *strIdPtr               = strId;
      HOP_STRNCPY(
          &ctxt->stringData[newEntryPos + sizeof( hop_str_ptr_t )], strPtr, alignedStrLen );
   }
   return inserted;
}

static hop_str_ptr_t add_string_to_db( const char* strId )
{
   // Early return on NULL
   if( strId == 0 ) return 0;

   return add_string_to_db_internal( &tl_context, strId, strId, strlen( strId ) );
}

static hop_str_ptr_t add_dynamic_string_to_db( const char* dynStr )
{
   // Should not have null as dyn string, but just in case...
   if( dynStr == NULL ) return 0;

   const size_t strLen = strlen( dynStr );
   const hop_str_ptr_t hash = c_str_hash( dynStr, strLen );
   return add_string_to_db_internal( &tl_context, hash, dynStr, strLen );
}

static void
enter_internal( hop_timestamp_t ts, const char* fileName, hop_linenb_t line, const char* fctName )
{
   const uint32_t curCount = tl_context.pendingTraces.count;
   if( curCount == tl_context.pendingTraces.maxSize )
   {
      alloc_traces( &tl_context.pendingTraces, tl_context.pendingTraces.maxSize * 2 );
   }

   // Keep the index of the last opened trace in the new 'end' timestamp. It will
   // be restored when poping the trace
   tl_context.pendingTraces.ends[curCount]        = tl_context.openTraceIdx;
   tl_context.openTraceIdx                        = curCount;

   // Save the actual data
   tl_context.pendingTraces.starts[curCount]      = ts;
   tl_context.pendingTraces.depths[curCount]      = tl_context.traceLevel++;
   tl_context.pendingTraces.fileNameIds[curCount] = (hop_str_ptr_t)fileName;
   tl_context.pendingTraces.fctNameIds[curCount]  = (hop_str_ptr_t)fctName;
   tl_context.pendingTraces.lineNumbers[curCount] = line;
   tl_context.pendingTraces.zones[curCount]       = tl_context.zoneId;
   ++tl_context.pendingTraces.count;
}

void hop_enter( const char* fileName, hop_linenb_t line, const char* fctName )
{
   if( HOP_UNLIKELY( !thread_local_context_valid( &tl_context ) ) )
   {
      thread_local_context_create( &tl_context );
   }

   enter_internal( hop_get_timestamp_no_core(), fileName, line, fctName );
}

void hop_enter_dynamic_string( const char* fileName, hop_linenb_t line, const char* fctName )
{
   if( HOP_UNLIKELY( !thread_local_context_valid( &tl_context ) ) )
   {
     thread_local_context_create( &tl_context );
   }

   // Flag the timestamp as being dynamic (first bit set), and add the dynamic string to the db
   enter_internal(
       hop_get_timestamp_no_core() | 1ULL, fileName, line, add_dynamic_string_to_db( fctName ) );
}

void hop_leave()
{
   const hop_timestamp_t endTime           = hop_get_timestamp_no_core();
   const int32_t remainingPushedTraces     = --tl_context.traceLevel;
   const uint32_t lastOpenTraceIdx         = tl_context.openTraceIdx;
   tl_context.openTraceIdx                         = tl_context.pendingTraces.ends[lastOpenTraceIdx];
   tl_context.pendingTraces.ends[lastOpenTraceIdx] = endTime;

   if( remainingPushedTraces <= 0 )
   {
      HOP_ASSERT( remainingPushedTraces == 0 ); // If < 0, there is a mismatch of enter/leave calls
      // client->flushToConsumer();

      // client->addProfilingTrace( fileName, fctName, start, end, lineNb, zone );
      // client->addCoreEvent( core, start, end );
   }
}

static void* create_ipc_memory(
    const HOP_CHAR* path,
    uint64_t size,
    shm_handle* handle,
    hop_connection_state* state )
{
   uint8_t* sharedMem = NULL;
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
      *state = err_to_connection_state( GetLastError() );
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
      *state = err_to_connection_state( GetLastError() );
      CloseHandle( *handle );
      return NULL;
   }
#else
   *handle = shm_open( path, O_CREAT | O_RDWR, 0666 );
   if( *handle < 0 )
   {
      *state = err_to_connection_state( errno );
      return NULL;
   }

   int truncRes = ftruncate( *handle, size );
   if( truncRes != 0 )
   {
      *state = err_to_connection_state( errno );
      return NULL;
   }

   sharedMem = (uint8_t*)( mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 ) );
#endif
   if( sharedMem ) *state = HOP_CONNECTED;
   return sharedMem;
}

static void* open_ipc_memory(
    const HOP_CHAR* path,
    shm_handle* handle,
    uint64_t* totalSize,
    hop_connection_state* state )
{
   uint8_t* sharedMem = NULL;
#if defined( _MSC_VER )
   *handle = OpenFileMapping(
       FILE_MAP_ALL_ACCESS,  // read/write access
       FALSE,                // do not inherit the name
       path );               // name of mapping object

   if( *handle == NULL )
   {
      *state = err_to_connection_state( GetLastError() );
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
      *state = err_to_connection_state( GetLastError() );
      CloseHandle( *handle );
      return NULL;
   }

   MEMORY_BASIC_INFORMATION memInfo;
   if( !VirtualQuery( sharedMem, &memInfo, sizeof( memInfo ) ) )
   {
      *state = err_to_connection_state( GetLastError() );
      UnmapViewOfFile( sharedMem );
      CloseHandle( *handle );
      return NULL;
   }
   *totalSize = memInfo.RegionSize;
#else
   *handle = shm_open( path, O_RDWR, 0666 );
   if( *handle < 0 )
   {
      *state = err_to_connection_state( errno );
      return NULL;
   }

   struct stat fileStat;
   if( fstat( *handle, &fileStat ) < 0 )
   {
      *state = err_to_connection_state( errno );
      return NULL;
   }

   *totalSize = fileStat.st_size;

   sharedMem =
       (uint8_t*)( mmap( NULL, fileStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, *handle, 0 ) );
   *state = sharedMem ? HOP_CONNECTED : HOP_UNKNOWN_CONNECTION_ERROR;
#endif
   return sharedMem;
}

static void close_ipc_memory( const HOP_CHAR* name, shm_handle handle, void* dataPtr )
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

static hop_connection_state err_to_connection_state( uint32_t err )
{
#if defined( _MSC_VER )
   if( err == ERROR_FILE_NOT_FOUND ) return HOP_NOT_CONNECTED;
   if( err == ERROR_ACCESS_DENIED ) return HOP_PERMISSION_DENIED;
   return HOP_UNKNOWN_CONNECTION_ERROR;
#else
   if( err == ENOENT ) return HOP_NOT_CONNECTED;
   if( err == EACCES ) return HOP_PERMISSION_DENIED;
   return HOP_UNKNOWN_CONNECTION_ERROR;
#endif
}

static uint32_t atomic_set_bit( hop_atomic_uint32* value, uint32_t bitToSet )
{
   uint32_t origValue = atomic_load_explicit( value, memory_order_seq_cst );
   while( !atomic_compare_exchange_weak( value, &origValue, origValue | bitToSet ) )
      ;
   return origValue;  // return value before change
}

static uintptr_t atomic_clear_bit( hop_atomic_uint32* value, uint32_t bitToSet )
{
   const uint32_t mask = ~bitToSet;
   uint32_t origValue = atomic_load_explicit( value, memory_order_seq_cst );
   while( !atomic_compare_exchange_weak( value, &origValue, origValue & mask ) )
      ;
   return origValue;  // return value before change
}

static hop_bool_t has_connected_producer( hop_shared_memory* mem )
{
   return ( mem->ipcSegment->state & HOP_CONNECTED_PRODUCER ) > 0;
}

static void set_connected_producer( hop_shared_memory* mem, hop_bool_t connected )
{
   if( connected )
      atomic_set_bit( &mem->ipcSegment->state, HOP_CONNECTED_PRODUCER );
   else
      atomic_clear_bit( &mem->ipcSegment->state, HOP_CONNECTED_PRODUCER );
}

static hop_bool_t has_connected_consumer( hop_shared_memory* mem )
{
   return ( mem->ipcSegment->state & HOP_CONNECTED_CONSUMER ) > 0;
}

static void set_connected_consumer( hop_shared_memory* mem, hop_bool_t connected )
{
   if( connected )
      atomic_set_bit( &mem->ipcSegment->state, HOP_CONNECTED_CONSUMER );
   else
      atomic_clear_bit( &mem->ipcSegment->state, HOP_CONNECTED_CONSUMER );
}

static hop_bool_t has_listening_consumer( hop_shared_memory* mem )
{
   const uint32_t mask = HOP_CONNECTED_CONSUMER | HOP_LISTENING_CONSUMER;
   return ( mem->ipcSegment->state & mask ) == mask;
}

static void set_listening_consumer( hop_shared_memory* mem, hop_bool_t listening )
{
   if( listening )
      atomic_set_bit( &mem->ipcSegment->state, HOP_LISTENING_CONSUMER );
   else
      atomic_clear_bit( &mem->ipcSegment->state, HOP_LISTENING_CONSUMER );
}

static void set_reset_timestamp( hop_shared_memory* mem, hop_timestamp_t t )
{
   atomic_store_explicit( &mem->ipcSegment->lastResetTimeStamp, t, memory_order_seq_cst );
}

static hop_bool_t should_send_heartbeat( hop_shared_memory* mem, hop_timestamp_t curTimestamp )
{
   // When a profiled app is open, in the viewer but not listed to, we would spam
   // unnecessary heartbeats every time a trace stack was sent. This make sure we only
   // send them every few milliseconds
   static const uint64_t cyclesBetweenHB = 100000000;
   const int64_t lastHb                  = atomic_load_explicit(
       &mem->ipcSegment->lastHeartbeatTimeStamp, memory_order_seq_cst );
   return curTimestamp - lastHb > cyclesBetweenHB;
}

static void set_last_heartbeat( hop_shared_memory* mem, hop_timestamp_t t )
{
   atomic_store_explicit( &mem->ipcSegment->lastHeartbeatTimeStamp, t, memory_order_seq_cst );
}

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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

/*
 * Branch prediction macros.
 */
#ifndef __predict_true
#define	__predict_true(x)	__builtin_expect((x) != 0, 1)
#define	__predict_false(x)	__builtin_expect((x) != 0, 0)
#endif

  /*
   * Exponential back-off for the spinning paths.
   */
#define	SPINLOCK_BACKOFF_MIN	4
#define	SPINLOCK_BACKOFF_MAX	128
#if defined(__x86_64__) || defined(__i386__)
#define SPINLOCK_BACKOFF_HOOK	__asm volatile("pause" ::: "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define	SPINLOCK_BACKOFF(count)					\
do {								\
	for (int __i = (count); __i != 0; __i--) {		\
		SPINLOCK_BACKOFF_HOOK;				\
	}							\
	if ((count) < SPINLOCK_BACKOFF_MAX)			\
		(count) += (count);				\
} while (/* CONSTCOND */ 0);


#define	RBUF_OFF_MASK	(0x00000000ffffffffUL)
#define	WRAP_LOCK_BIT	(0x8000000000000000UL)
#define	RBUF_OFF_MAX	(UINT64_MAX & ~WRAP_LOCK_BIT)

#define	WRAP_COUNTER	(0x7fffffff00000000UL)
#define	WRAP_INCR(x)	(((x) + 0x100000000UL) & WRAP_COUNTER)

/*
 * ringbuf_setup: initialise a new ring buffer of a given length.
 */
int
ringbuf_setup(ringbuf_t* rbuf, unsigned nworkers, size_t length)
{
   if (length >= RBUF_OFF_MASK) {
      errno = EINVAL;
      return -1;
   }
   memset(rbuf, 0, offsetof(ringbuf_t, workers[nworkers]));
   rbuf->space = length;
   rbuf->end = RBUF_OFF_MAX;
   rbuf->nworkers = nworkers;
   return 0;
}

/*
 * ringbuf_get_sizes: return the sizes of the ringbuf_t and ringbuf_worker_t.
 */
void
ringbuf_get_sizes(unsigned nworkers,
   size_t* ringbuf_size, size_t* ringbuf_worker_size)
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
ringbuf_worker_t*
ringbuf_register(ringbuf_t* rbuf, unsigned i)
{
   ringbuf_worker_t* w = &rbuf->workers[i];

   w->seen_off = RBUF_OFF_MAX;
   atomic_store_explicit(&w->registered, true, memory_order_release);
   return w;
}

void
ringbuf_unregister(ringbuf_t* rbuf, ringbuf_worker_t* w)
{
   w->registered = false;
   (void)rbuf;
}

/*
 * stable_nextoff: capture and return a stable value of the 'next' offset.
 */
static inline ringbuf_off_t
stable_nextoff(ringbuf_t* rbuf)
{
   unsigned count = SPINLOCK_BACKOFF_MIN;
   ringbuf_off_t next;
retry:
   next = atomic_load_explicit(&rbuf->next, memory_order_acquire);
   if (next & WRAP_LOCK_BIT) {
      SPINLOCK_BACKOFF(count);
      goto retry;
   }
   HOP_ASSERT((next & RBUF_OFF_MASK) < rbuf->space);
   return next;
}

/*
 * stable_seenoff: capture and return a stable value of the 'seen' offset.
 */
static inline ringbuf_off_t
stable_seenoff(ringbuf_worker_t* w)
{
   unsigned count = SPINLOCK_BACKOFF_MIN;
   ringbuf_off_t seen_off;
retry:
   seen_off = atomic_load_explicit(&w->seen_off, memory_order_acquire);
   if (seen_off & WRAP_LOCK_BIT) {
      SPINLOCK_BACKOFF(count);
      goto retry;
   }
   return seen_off;
}

/*
 * ringbuf_acquire: request a space of a given length in the ring buffer.
 *
 * => On success: returns the offset at which the space is available.
 * => On failure: returns -1.
 */
ssize_t
ringbuf_acquire(ringbuf_t* rbuf, ringbuf_worker_t* w, size_t len)
{
   ringbuf_off_t seen, next, target;

   HOP_ASSERT(len > 0 && len <= rbuf->space);
   HOP_ASSERT(w->seen_off == RBUF_OFF_MAX);

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
      HOP_ASSERT(next < rbuf->space);
      atomic_store_explicit(&w->seen_off, next | WRAP_LOCK_BIT,
         memory_order_relaxed);

      /*
       * Compute the target offset.  Key invariant: we cannot
       * go beyond the WRITTEN offset or catch up with it.
       */
      target = next + len;
      written = rbuf->written;
      if (__predict_false(next < written && target >= written)) {
         /* The producer must wait. */
         atomic_store_explicit(&w->seen_off,
            RBUF_OFF_MAX, memory_order_release);
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
            atomic_store_explicit(&w->seen_off,
               RBUF_OFF_MAX, memory_order_release);
            return -1;
         }
         /* Increment the wrap-around counter. */
         target |= WRAP_INCR(seen & WRAP_COUNTER);
      }
      else {
         /* Preserve the wrap-around counter. */
         target |= seen & WRAP_COUNTER;
      }
   } while (!atomic_compare_exchange_weak(&rbuf->next, &seen, target));

   /*
    * Acquired the range.  Clear WRAP_LOCK_BIT in the 'seen' value
    * thus indicating that it is stable now.
    *
    * No need for memory_order_release, since CAS issued a fence.
    */
   atomic_store_explicit(&w->seen_off, w->seen_off & ~WRAP_LOCK_BIT,
      memory_order_relaxed);

   /*
    * If we set the WRAP_LOCK_BIT in the 'next' (because we exceed
    * the remaining space and need to wrap-around), then save the
    * 'end' offset and release the lock.
    */
   if (__predict_false(target & WRAP_LOCK_BIT)) {
      /* Cannot wrap-around again if consumer did not catch-up. */
      HOP_ASSERT(rbuf->written <= next);
      HOP_ASSERT(rbuf->end == RBUF_OFF_MAX);
      rbuf->end = next;
      next = 0;

      /*
       * Unlock: ensure the 'end' offset reaches global
       * visibility before the lock is released.
       */
      atomic_store_explicit(&rbuf->next,
         (target & ~WRAP_LOCK_BIT), memory_order_release);
   }
   HOP_ASSERT((target & RBUF_OFF_MASK) <= rbuf->space);
   return (ssize_t)next;
}

/*
 * ringbuf_produce: indicate the acquired range in the buffer is produced
 * and is ready to be consumed.
 */
void
ringbuf_produce(ringbuf_t* rbuf, ringbuf_worker_t* w)
{
   (void)rbuf;
   HOP_ASSERT(w->registered);
   HOP_ASSERT(w->seen_off != RBUF_OFF_MAX);
   atomic_store_explicit(&w->seen_off, RBUF_OFF_MAX, memory_order_release);
}

/*
 * ringbuf_consume: get a contiguous range which is ready to be consumed.
 */
size_t
ringbuf_consume(ringbuf_t* rbuf, size_t* offset)
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
      ringbuf_worker_t* w = &rbuf->workers[i];
      ringbuf_off_t seen_off;

      /*
       * Skip if the worker has not registered.
       *
       * Get a stable 'seen' value.  This is necessary since we
       * want to discard the stale 'seen' values.
       */
      if (!atomic_load_explicit(&w->registered, memory_order_relaxed))
         continue;
      seen_off = stable_seenoff(w);

      /*
       * Ignore the offsets after the possible wrap-around.
       * We are interested in the smallest seen offset that is
       * not behind the 'written' offset.
       */
      if (seen_off >= written) {
         ready = HOP_MIN(seen_off, ready);
      }
      HOP_ASSERT(ready >= written);
   }

   /*
    * Finally, we need to determine whether wrap-around occurred
    * and deduct the safe 'ready' offset.
    */
   if (next < written) {
      const ringbuf_off_t end = HOP_MIN(rbuf->space, rbuf->end);

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
         }

         /*
          * Wrap-around the consumer and start from zero.
          */
         written = 0;
         atomic_store_explicit(&rbuf->written,
            written, memory_order_release);
         goto retry;
      }

      /*
       * We cannot wrap-around yet; there is data to consume at
       * the end.  The ready range is smallest of the observed
       * 'ready' or the 'end' offset.  If neither is set, then
       * the actual end of the buffer.
       */
      HOP_ASSERT(ready > next);
      ready = HOP_MIN(ready, end);
      HOP_ASSERT(ready >= written);
   }
   else {
      /*
       * Regular case.  Up to the observed 'ready' (if set)
       * or the 'next' offset.
       */
      ready = HOP_MIN(ready, next);
   }
   towrite = ready - written;
   *offset = written;

   HOP_ASSERT(ready >= written);
   HOP_ASSERT(towrite <= rbuf->space);
   return towrite;
}

/*
 * ringbuf_release: indicate that the consumed range can now be released.
 */
void
ringbuf_release(ringbuf_t* rbuf, size_t nbytes)
{
   const size_t nwritten = rbuf->written + nbytes;

   HOP_ASSERT(rbuf->written <= rbuf->space);
   HOP_ASSERT(rbuf->written <= rbuf->end);
   HOP_ASSERT(nwritten <= rbuf->space);

   rbuf->written = (nwritten == rbuf->space) ? 0 : nwritten;
}


/**
 * Hash set implementation
 */

/**
 * Start of hop hash set implementation
 */

static const uint32_t DEFAULT_TABLE_SIZE = 1 << 8U;  // Required to be a power of 2 !
static const float MAX_LOAD_FACTOR       = 0.4f;

typedef struct hop_hash_set
{
   const void** table;
   uint32_t capacity;
   uint32_t count;
} hop_hash_set;

static inline float load_factor( hop_hash_set_t set ) { return (float)set->count / set->capacity; }

static inline uint64_t hash_func( const void* value )
{
   return (uint64_t)value;
}

static inline uint32_t quad_probe( uint64_t hash_value, uint32_t it, uint32_t table_size )
{
   // Using quadratic probing function (x^2 + x) / 2
   return ( hash_value + ( ( it * it + it ) >> 2 ) ) % table_size;
}

// Insert value inside the hash set without incrementing the count. Used while rehashing as
// well as within the public insert function
static int insert_internal( hop_hash_set_t hs, const void* value )
{
   const uint64_t hash_value = hash_func( value );
   uint32_t iteration        = 0;
   while( iteration < hs->capacity )
   {
      const uint32_t idx         = quad_probe( hash_value, iteration++, hs->capacity );
      const void* existing_value = hs->table[idx];
      if( existing_value == value )
      {
         return 0;  // Value already inserted. Return insertion failure
      }
      else if( existing_value == NULL )
      {
         hs->table[idx] = value;
         return 1;
      }
   }

   return 0;
}

static void rehash( hop_hash_set_t hs )
{
   const void** prev_table            = hs->table;
   const uint32_t prev_capacity = hs->capacity;

   hs->capacity = prev_capacity * 2;
   hs->table    = (const void**)calloc( hs->capacity, sizeof( const void* ) );

   for( uint32_t i = 0; i < prev_capacity; ++i )
   {
      if( prev_table[i] != NULL ) insert_internal( hs, prev_table[i] );
   }

   free( prev_table );
}

hop_hash_set_t hop_hash_set_create()
{
   hop_hash_set* hs = (hop_hash_set*)calloc( 1, sizeof( hop_hash_set ) );
   if( !hs ) return NULL;

   hs->table = (const void**)calloc( DEFAULT_TABLE_SIZE, sizeof( const void* ) );
   if( !hs->table )
   {
      free( hs );
      return NULL;
   }

   hs->capacity = DEFAULT_TABLE_SIZE;
   return hs;
}

void hop_hash_set_destroy( hop_hash_set_t set )
{
   if( set )
   {
      free( set->table );
   }
   free( set );
}

int hop_hash_set_insert( hop_hash_set_t hs, const void* value )
{
   const int inserted = insert_internal( hs, value );
   if( inserted )
   {
      ++hs->count;
      if( load_factor( hs ) > MAX_LOAD_FACTOR )
      {
         rehash( hs );
      }
   }
   return inserted;
}

int hop_hash_set_count( hop_hash_set_t set ) { return set->count; }

void hop_hash_set_clear( hop_hash_set_t set )
{
   if( set )
   {
      set->count = 0;
      if( set->table )
      {
         memset( set->table, 0, set->capacity * sizeof( const void* ) );
      }
   }
}





















#if 0


class Client;
class SharedMemory;

class HOP_API ClientManager
{
  public:
   static Client* Get();
   static hop_zone_t StartProfile();
   static hop_str_ptr_t StartProfileDynString( const char*, hop_zone_t* );
   static void EndProfile(
       hop_str_ptr_t fileName,
       hop_str_ptr_t fctName,
       hop_timestamp_t start,
       hop_timestamp_t end,
       hop_linenb_t lineNb,
       hop_zone_t zone,
       hop_core_t core );
   static void EndLockWait( void* mutexAddr, hop_timestamp_t start, hop_timestamp_t end );
   static void UnlockEvent( void* mutexAddr, hop_timestamp_t time );
   static void SetThreadName( const char* name ) HOP_NOEXCEPT;
   static hop_zone_t PushNewZone( hop_zone_t newZone );
   
   static SharedMemory& sharedMemory() HOP_NOEXCEPT;
};

class ProfGuard
{
  public:
   ProfGuard( const char* fileName, hop_linenb_t lineNb, const char* fctName ) HOP_NOEXCEPT
   {
      open( fileName, lineNb, fctName );
   }
   ~ProfGuard() { close(); }
   inline void reset( const char* fileName, hop_linenb_t lineNb, const char* fctName )
   {
      // Please uncomment the following line if close() is made public!
      // if ( _fctName )
      close();
      open( fileName, lineNb, fctName );
   }

  private:
   inline void open( const char* fileName, hop_linenb_t lineNb, const char* fctName )
   {
      _start    = getTimeStamp();
      _fileName = (hop_str_ptr_t)fileName;
      _fctName  = (hop_str_ptr_t)fctName;
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

   hop_timestamp_t _start;
   hop_str_ptr_t _fileName, _fctName;
   hop_linenb_t _lineNb;
   hop_zone_t _zone;
};

class LockWaitGuard
{
   hop_timestamp_t start;
   void* mutexAddr;
  public:
   LockWaitGuard( void* mutAddr ) : start( getTimeStamp() ), mutexAddr( mutAddr ) {}
   ~LockWaitGuard() { ClientManager::EndLockWait( mutexAddr, start, getTimeStamp() ); }
};

class ProfGuardDynamicString
{
  public:
   ProfGuardDynamicString( const char* fileName, hop_linenb_t lineNb, const char* fctName ) HOP_NOEXCEPT
       : _start( getTimeStamp() | 1ULL ),  // Set the first bit to 1 to signal dynamic strings
         _fileName( reinterpret_cast<hop_str_ptr_t>( fileName ) ),
         _lineNb( lineNb )
   {
      _fctName = ClientManager::StartProfileDynString( fctName, &_zone );
   }
   ~ProfGuardDynamicString()
   {
      ClientManager::EndProfile( _fileName, _fctName, _start, getTimeStamp(), _lineNb, _zone, 0 );
   }

  private:
   hop_timestamp_t _start;
   hop_str_ptr_t _fileName;
   hop_str_ptr_t _fctName;
   hop_linenb_t _lineNb;
   hop_zone_t _zone;
};

class ZoneGuard
{
  public:
   ZoneGuard( hop_zone_t newZone ) HOP_NOEXCEPT
   {
      _prevZoneId = ClientManager::PushNewZone( newZone );
   }
   ~ZoneGuard() { ClientManager::PushNewZone( _prevZoneId ); }

  private:
   hop_zone_t _prevZoneId;
};

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


}  // namespace hop

typedef struct hop_hash_set* hop_hash_set_t;
hop_hash_set_t hop_hash_set_create();
void hop_hash_set_destroy( hop_hash_set_t set );
void hop_hash_set_clear( hop_hash_set_t set );
int hop_hash_set_insert( hop_hash_set_t set, const void* value );

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
class SharedMemory
{
  public:
   enum ConnectionState
   {
      NO_TARGET_PROCESS,
      NOT_CONNECTED,
      CONNECTED,
      CONNECTED_NO_CLIENT,
      PERMISSION_DENIED,
      INVALID_VERSION,
      UNKNOWN_CONNECTION_ERROR
   };

   ConnectionState create( int pid, size_t size, bool isConsumer );
   void destroy();

   bool shouldSendHeartbeat( hop_timestamp_t t ) const HOP_NOEXCEPT;
   void setLastHeartbeatTimestamp( hop_timestamp_t t ) HOP_NOEXCEPT;
   hop_timestamp_t lastResetTimestamp() const HOP_NOEXCEPT;
   void setResetTimestamp( hop_timestamp_t t ) HOP_NOEXCEPT;
   ringbuf_t* ringbuffer() const HOP_NOEXCEPT;
   uint8_t* data() const HOP_NOEXCEPT;
   bool valid() const HOP_NOEXCEPT;
   int pid() const HOP_NOEXCEPT;
   const SharedMetaInfo* sharedMetaInfo() const HOP_NOEXCEPT;
   ~SharedMemory();

  private:
   // Pointer into the shared memory
   SharedMetaInfo* _sharedMetaData{NULL};
   ringbuf_t* _ringbuf{NULL};
   uint8_t* _data{NULL};
   // ----------------
   bool _isConsumer{false};
   shm_handle _sharedMemHandle{};
   int _pid;
   HOP_CHAR _sharedMemPath[HOP_SHARED_MEM_MAX_NAME_SIZE];
   std::atomic<bool> _valid{false};
   std::mutex _creationMutex;
};
}  // namespace hop
#endif  // defined(HOP_VIEWER)

/* ======================================================================
                    End of private declarations
   ==================================================================== */

#if defined( HOP_IMPLEMENTATION )

// standard includes
#include <HOP_ASSERT.h>

#define HOP_MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define HOP_MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define HOP_UNUSED( x ) (void)( x )

#if !defined( _MSC_VER )

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = "/hop_";
/*
// Unix shared memory includes
#include <fcntl.h>     // O_CREAT
#include <cstring>     // memcpy
#include <pthread.h>   // pthread_self
#include <sys/mman.h>  // shm_open
#include <sys/stat.h>  // stat
#include <unistd.h>    // ftruncate

#define HOP_STRLEN( str ) strlen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy( ( dst ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat( ( dst ), ( src ), ( count ) )

#define HOP_LIKELY( x ) __builtin_expect( !!( x ), 1 )
#define HOP_UNLIKELY( x ) __builtin_expect( !!( x ), 0 )

#define HOP_GET_THREAD_ID() reinterpret_cast<size_t>( pthread_self() )
#define HOP_SLEEP_MS( x ) usleep( x * 1000 )
*/
inline int HOP_GET_PID() { return getpid(); }

#else  // !defined( _MSC_VER )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const HOP_CHAR HOP_SHARED_MEM_PREFIX[] = _T("/hop_");
/*

#define HOP_STRLEN( str ) _tcslen( ( str ) )
#define HOP_STRNCPYW( dst, src, count ) _tcsncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCATW( dst, src, count ) _tcsncat_s( ( dst ), ( src ), ( count ) )
#define HOP_STRNCPY( dst, src, count ) strncpy_s( ( dst ), ( count ), ( src ), ( count ) )
#define HOP_STRNCAT( dst, src, count ) strncat_s( ( dst ), ( count ), ( src ), ( count ) )

#define HOP_LIKELY( x ) ( x )
#define HOP_UNLIKELY( x ) ( x )

#define HOP_GET_THREAD_ID() ( size_t ) GetCurrentThreadId()
#define HOP_SLEEP_MS( x ) Sleep( x )

inline int HOP_GET_PID() { return GetCurrentProcessId(); }
*/
#endif  // !defined( _MSC_VER )

namespace
{

void* createSharedMemory(
    const HOP_CHAR* path,
    uint64_t size,
    shm_handle* handle,
    hop::SharedMemory::ConnectionState* state )
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
   *handle = shm_open( path, O_CREAT | O_RDWR, 0666 );
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
   if( sharedMem ) *state = hop::SharedMemory::CONNECTED;
   return sharedMem;
}

void* openSharedMemory(
    const HOP_CHAR* path,
    shm_handle* handle,
    uint64_t* totalSize,
    hop::SharedMemory::ConnectionState* state )
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
   *handle = shm_open( path, O_RDWR, 0666 );
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
   *state = sharedMem ? hop::SharedMemory::CONNECTED : hop::SharedMemory::UNKNOWN_CONNECTION_ERROR;
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

static std::atomic<bool> g_done{false};  // Was the shared memory destroyed? (Are we done?)


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
         set_listening_consumer( false );
         set_connected_consumer( false );
      }
      else
      {
         set_connected_producer( false );
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
      g_done.store( true );
   }
}

SharedMemory::~SharedMemory() { destroy(); }

static void addTrace(
    Traces* t,
    hop_timestamp_t start,
    hop_timestamp_t end,
    hop_depth_t depth,
    hop_str_ptr_t fileName,
    hop_str_ptr_t fctName,
    hop_linenb_t lineNb,
    hop_zone_t zone )
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
   const size_t sliceSize = sizeof( hop_timestamp_t ) * 2 + +sizeof( hop_depth_t ) + sizeof( hop_str_ptr_t ) * 2 +
                            sizeof( hop_linenb_t ) + sizeof( hop_zone_t );
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


      _stringPtrSet = hop_hash_set_create();

      resetStringData();
   }

   ~Client()
   {
      freeTraces( &_traces );
      hop_hash_set_destroy( _stringPtrSet );
   }

   void addProfilingTrace(
       hop_str_ptr_t fileName,
       hop_str_ptr_t fctName,
       hop_timestamp_t start,
       hop_timestamp_t end,
       hop_linenb_t lineNb,
       hop_zone_t zone )
   {
      addTrace( &_traces, start, end, (hop_depth_t)tl_traceLevel, fileName, fctName, lineNb, zone );
   }

   void addCoreEvent( hop_core_t core, hop_timestamp_t startTime, hop_timestamp_t endTime )
   {
      _cores.emplace_back( CoreEvent{startTime, endTime, core} );
   }

   void addWaitLockTrace( void* mutexAddr, hop_timestamp_t start, hop_timestamp_t end, hop_depth_t depth )
   {
      _lockWaits.push_back( LockWait{mutexAddr, start, end, depth, 0 /*padding*/} );
   }

   void addUnlockEvent( void* mutexAddr, hop_timestamp_t time )
   {
      _unlockEvents.push_back( UnlockEvent{mutexAddr, time} );
   }

   void setThreadName( hop_str_ptr_t name )
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

   bool addStringToDb( hop_str_ptr_t strId )
   {
      // Early return on NULL
      if( strId == 0 ) return false;

      const hop_bool_t inserted = hop_hash_set_insert( _stringPtrSet, (void*)strId );
      // If the string was inserted (meaning it was not already there),
      // add it to the database, otherwise do nothing
      if( inserted )
      {
         const size_t newEntryPos = _stringData.size();
         HOP_ASSERT( ( newEntryPos & 7 ) == 0 );  // Make sure we are 8 byte aligned

         const size_t alignedStrLen = alignOn(
             static_cast<uint32_t>( strlen( reinterpret_cast<const char*>( strId ) ) ) + 1, 8 );

         _stringData.resize( newEntryPos + sizeof( hop_str_ptr_t ) + alignedStrLen );
         hop_str_ptr_t* strIdPtr = reinterpret_cast<hop_str_ptr_t*>( &_stringData[newEntryPos] );
         *strIdPtr          = strId;
         HOP_STRNCPY(
             &_stringData[newEntryPos + sizeof( hop_str_ptr_t )],
             reinterpret_cast<const char*>( strId ),
             alignedStrLen );
      }

      return inserted;
   }

   void resetStringData()
   {
      hop_hash_set_clear( _stringPtrSet );
      _stringData.clear();
      _sentStringDataSize   = 0;
      _clientResetTimeStamp = ClientManager::sharedMemory().lastResetTimestamp();

      // Push back thread name
      if( tl_threadNameBuffer[0] != '\0' )
      {
         const auto hash = addDynamicStringToDb( tl_threadNameBuffer );
         HOP_UNUSED( hash );
         HOP_ASSERT( hash == tl_threadName );
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

   bool sendStringData( hop_timestamp_t timeStamp )
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
      HOP_ASSERT( stringDataSize >= _sentStringDataSize );
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
         msgInfo->timeStamp       = timeStamp;
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

   bool sendTraces( hop_timestamp_t timeStamp )
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
         tracesInfo->timeStamp    = timeStamp;
         tracesInfo->traces.count = _traces.count;

         // Copy trace information into buffer to send
         void* outBuffer = (void*)( bufferPtr + sizeof( MsgInfo ) );
         copyTracesTo( &_traces, outBuffer );
      }

      ringbuf_produce( ringbuf, _worker );

      _traces.count = 0;

      return true;
   }

   bool sendCores( hop_timestamp_t timeStamp )
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
         coreInfo->timeStamp        = timeStamp;
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

   bool sendLockWaits( hop_timestamp_t timeStamp )
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
         lwInfo->timeStamp       = timeStamp;
         lwInfo->lockwaits.count = static_cast<uint32_t>( _lockWaits.size() );
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _lockWaits.data(), _lockWaits.size() * sizeof( LockWait ) );
      }

      ringbuf_produce( ringbuf, _worker );

      _lockWaits.clear();

      return true;
   }

   bool sendUnlockEvents( hop_timestamp_t timeStamp )
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
         uInfo->timeStamp          = timeStamp;
         uInfo->unlockEvents.count = static_cast<uint32_t>( _unlockEvents.size() );
         bufferPtr += sizeof( MsgInfo );
         memcpy( bufferPtr, _unlockEvents.data(), _unlockEvents.size() * sizeof( UnlockEvent ) );
      }

      ringbuf_produce( ringbuf, _worker );

      _unlockEvents.clear();

      return true;
   }

   bool sendHeartbeat( hop_timestamp_t timeStamp )
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
         hbInfo->timeStamp   = timeStamp;
         bufferPtr += sizeof( MsgInfo );
      }

      ringbuf_produce( ringbuf, _worker );

      return true;
   }

   void flushToConsumer()
   {
      const hop_timestamp_t timeStamp = getTimeStamp();

      // If we have a consumer, send life signal
      if( ClientManager::has_connected_consumer() && ClientManager::ShouldSendHeartbeat( timeStamp ) )
      {
         sendHeartbeat( timeStamp );
      }

      // If no one is there to listen, no need to send any data
      if( ClientManager::has_listening_consumer() )
      {
         // If the shared memory reset timestamp more recent than our local one
         // it means we need to clear our string table. Otherwise it means we
         // already took care of it. Since some traces might depend on strings
         // that were added dynamically (ie before clearing the db), we cannot
         // consider them and need to return here.
         hop_timestamp_t resetTimeStamp = ClientManager::sharedMemory().lastResetTimestamp();
         if( _clientResetTimeStamp < resetTimeStamp )
         {
            resetStringData();
            resetPendingTraces();
            return;
         }

         sendStringData( timeStamp );  // Always send string data first
         sendTraces( timeStamp );
         sendLockWaits( timeStamp );
         sendUnlockEvents( timeStamp );
         sendCores( timeStamp );
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
   hop_hash_set_t _stringPtrSet;
   std::vector<char> _stringData;
   hop_timestamp_t _clientResetTimeStamp{0};
   ringbuf_worker_t* _worker{NULL};
   uint32_t _sentStringDataSize{0};  // The size of the string array on viewer side
};

Client* ClientManager::Get()
{
   // thread_local std::unique_ptr<Client> threadClient;

   // if( HOP_UNLIKELY( g_done.load() ) ) return nullptr;
   // if( HOP_LIKELY( threadClient.get() ) ) return threadClient.get();

   // // If we have not yet created our shared memory segment, do it here
   // if( !ClientManager::sharedMemory().valid() )
   // {
   //    SharedMemory::ConnectionState state =
   //        ClientManager::sharedMemory().create( HOP_GET_PID(), HOP_SHARED_MEM_SIZE, false );
   //    if( state != SharedMemory::CONNECTED )
   //    {
   //       const char* reason = "";
   //       if( state == SharedMemory::PERMISSION_DENIED ) reason = " : Permission Denied";

   //       printf( "HOP - Could not create shared memory%s. HOP will not be able to run\n", reason );
   //       return NULL;
   //    }
   // }

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
         HOP_ASSERT( false && "ringbuf_register" );
      }
   }

   return threadClient.get();
}
hop_str_ptr_t ClientManager::StartProfileDynString( const char* str, hop_zone_t* zone )
{
   ++tl_traceLevel;
   Client* client = ClientManager::Get();

   if( HOP_UNLIKELY( !client ) ) return 0;

   *zone = tl_zoneId;
   return client->addDynamicStringToDb( str );
}

void ClientManager::EndProfile(
    hop_str_ptr_t fileName,
    hop_str_ptr_t fctName,
    hop_timestamp_t start,
    hop_timestamp_t end,
    hop_linenb_t lineNb,
    hop_zone_t zone,
    hop_core_t core )
{
   const int remainingPushedTraces = --tl_traceLevel;
   Client* client                  = ClientManager::Get();

   if( HOP_UNLIKELY( !client ) ) return;

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

void ClientManager::EndLockWait( void* mutexAddr, hop_timestamp_t start, hop_timestamp_t end )
{
   // Only add lock wait event if the lock is coming from within
   // measured code
   if( tl_traceLevel > 0 && end - start >= HOP_MIN_LOCK_CYCLES )
   {
      auto client = ClientManager::Get();
      if( HOP_UNLIKELY( !client ) ) return;

      client->addWaitLockTrace(
          mutexAddr, start, end, static_cast<unsigned short>( tl_traceLevel ) );
   }
}

void ClientManager::UnlockEvent( void* mutexAddr, hop_timestamp_t time )
{
   if( tl_traceLevel > 0 )
   {
      auto client = ClientManager::Get();
      if( HOP_UNLIKELY( !client ) ) return;

      client->addUnlockEvent( mutexAddr, time );
   }
}

void ClientManager::SetThreadName( const char* name ) HOP_NOEXCEPT
{
   auto client = ClientManager::Get();
   if( HOP_UNLIKELY( !client ) ) return;

   client->setThreadName( reinterpret_cast<hop_str_ptr_t>( name ) );
}

hop_zone_t ClientManager::PushNewZone( hop_zone_t newZone )
{
   hop_zone_t prevZone = tl_zoneId;
   tl_zoneId         = newZone;
   return prevZone;
}



bool ClientManager::ShouldSendHeartbeat( hop_timestamp_t t ) HOP_NOEXCEPT
{
   return ClientManager::sharedMemory().valid() &&
          ClientManager::sharedMemory().shouldSendHeartbeat( t );
}

void ClientManager::SetLastHeartbeatTimestamp( hop_timestamp_t t ) HOP_NOEXCEPT
{
   ClientManager::sharedMemory().setLastHeartbeatTimestamp( t );
}

SharedMemory& ClientManager::sharedMemory() HOP_NOEXCEPT
{
   static SharedMemory _sharedMemory;
   return _sharedMemory;
}

}  // end of namespace hop



#endif  // end HOP_IMPLEMENTATION




#endif










#endif // HOP_ENABLED

#ifdef __cplusplus
}
#endif

#endif // HOP_C_H_