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

#include <stdint.h>
#include <chrono>
#include <thread>

///////////////////////////////////////////////////////////////
/////    THESE ARE THE MACROS YOU SHOULD USE        ///////////
///////////////////////////////////////////////////////////////

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
/////       EVERYTHING AFTER IS IMPL DETAILS           ////////
///////////////////////////////////////////////////////////////

// ------ platform.h ------------
#define VDBG_CONSTEXPR constexpr
#define VDBG_NOEXCEPT noexcept
#define VDBG_STATIC_ASSERT static_assert
#define VDBG_GET_THREAD_ID() std::this_thread::get_id()
// -----------------------------

// ------ message.h ------------
namespace vdbg
{
namespace details
{
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
using Precision = std::chrono::nanoseconds;
inline auto getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

// Right now, I expect the trace to be 64 bytes. The name of the function should fit inside the rest
// of the 64 bytes that is not used. It could (and should) be optimized later so we
// do not send all of the null character on the socket when the name is smaller than the array
constexpr uint32_t EXPECTED_TRACE_SIZE = 64;
static constexpr const uint32_t MAX_FCT_NAME_LENGTH =
    EXPECTED_TRACE_SIZE - sizeof( unsigned char ) - 2 * sizeof( TimeStamp );

struct TracesInfo
{
   uint32_t threadId;
   uint32_t traceCount;
};
VDBG_STATIC_ASSERT( sizeof( TracesInfo ) == 8, "TracesInfo layout has changed unexpectedly" );

struct Trace
{
   TimeStamp start, end;
   unsigned char group;
   char name[MAX_FCT_NAME_LENGTH];
};
VDBG_STATIC_ASSERT(
    sizeof( Trace ) == EXPECTED_TRACE_SIZE,
    "Trace layout has changed unexpectedly" );

// ------ end of message.h ------------

// -------- vdbg_client.h -------------
static constexpr int MAX_THREAD_NB = 64;
class ClientProfiler
{
  public:
   class Impl;
   static Impl* Get( std::thread::id tId );
   static void StartProfile( Impl* );
   static void EndProfile(
       Impl*,
       const char* name,
       const char* classStr,
       TimeStamp start,
       TimeStamp end,
       uint8_t group );

  private:
   static size_t threadsId[MAX_THREAD_NB];
   static ClientProfiler::Impl* clientProfilers[MAX_THREAD_NB];
};

class ProfGuard
{
  public:
   ProfGuard( const char* name, const char* classStr, uint8_t groupId ) VDBG_NOEXCEPT
       : start( getTimeStamp() ),
         fctName( name ),
         className( classStr ),
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

#define VDBG_COMBINE( X, Y ) X##Y
#define VDBG_PROF_GUARD_VAR( LINE, ARGS ) \
   vdbg::details::ProfGuard VDBG_COMBINE( vdbgProfGuard, LINE ) ARGS

// -------- end of vdbg_client.h -------------

// ------ client.h ------------
class Client
{
  public:
   Client();
   ~Client();
   bool connect( const char* serverName );
   bool send( uint8_t* data, uint32_t size ) const;
   void disconnect();

  private:
   enum class State : uint8_t
   {
      NOT_CONNECTED = 0,
      CONNECTED,
      BROKEN_PIPE,
      ACCESS_ERROR,
      UNKNOWN_SHOULD_INVESTIGATE,
   };

   int _socket{-1};
   mutable State _state{State::NOT_CONNECTED};
};

}  // namespace details
}  // namespace vdbg

// End of vdbg declarations

#ifdef VDBG_IMPLEMENTATION

// C sockets include
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// C++ standard includes
#include <cassert>
#include <mutex>
#include <vector>

namespace vdbg
{
namespace details
{

// ------ client.cpp ------------

Client::Client()
{
   _socket = socket( AF_UNIX, SOCK_STREAM, 0 );
   if ( _socket < 0 )
   {
      perror( "socket() failed" );
   }
}

bool Client::connect( const char* serverName )
{
   struct sockaddr_un serveraddr;
   memset( &serveraddr, 0, sizeof( serveraddr ) );
   serveraddr.sun_family = AF_UNIX;
   strcpy( serveraddr.sun_path, serverName );

   int rc = ::connect( _socket, (struct sockaddr*)&serveraddr, SUN_LEN( &serveraddr ) );
   if ( rc < 0 )
   {
      // perror( "connect() failed" );
      return false;
   }

   _state = State::CONNECTED;
   return true;
}

bool Client::send( uint8_t* data, uint32_t size ) const
{
   if( _state != State::CONNECTED ) return false;

   int rc = ::send( _socket, data, size, MSG_NOSIGNAL );
   if ( rc < 0 )
   {
      switch ( errno )
      {
         case EPIPE:
            _state = State::BROKEN_PIPE;
            break;
         case EACCES:
            _state = State::ACCESS_ERROR;
            break;
         default:
            perror( "send() falied with unhandled error" );
            _state = State::UNKNOWN_SHOULD_INVESTIGATE;
            break;
      }
      return false;
   }

   return true;
}

Client::~Client() { ::close( _socket ); }

// ------ end of client.cpp ------------


// ------ cdbg_client.cpp------------

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
   Impl(size_t id) : _hashedThreadId( id )
   {
      _shallowTraces.reserve( 64 );
      _isClientConnected = _client.connect( "/tmp/my_server" );
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

   void flushToServer()
   {
      if( !_isClientConnected )
      {
         _isClientConnected = _client.connect("/tmp/my_server");
         if( !_isClientConnected )
         {
            // Cannot connect to server
            _shallowTraces.clear();
            return;
         }
      }

      // Allocate raw buffer to send to server
      const size_t traceMsgSize =
          sizeof( TracesInfo ) + sizeof( Trace ) * _shallowTraces.size();
      uint8_t* buffer = (uint8_t*)malloc( sizeof( MsgHeader ) + traceMsgSize );
      memset ( buffer, 0, sizeof( MsgHeader ) + traceMsgSize );

      MsgHeader* msgHeader = (MsgHeader*)buffer;
      TracesInfo* tracesInfo = (TracesInfo*)(buffer + sizeof( MsgHeader ) );
      Trace* traceToSend =
          (Trace*)( buffer + sizeof( MsgHeader ) + sizeof( TracesInfo ) );

      // Create the msg header first
      msgHeader->type = MsgType::PROFILER_TRACE;
      msgHeader->size = traceMsgSize;

      // TODO: Investigate if the truncation from size_t to uint32 is safe .. or not
      tracesInfo->threadId = (uint32_t)_hashedThreadId;
      tracesInfo->traceCount = (uint32_t)_shallowTraces.size();

      for( size_t i = 0; i < _shallowTraces.size(); ++i )
      {
         auto& t = traceToSend[i];
         t.start = _shallowTraces[i].start;
         t.end = _shallowTraces[i].end;
         t.group = _shallowTraces[i].group;

         // Copy the actual string into the buffer that will be transfer
         if( _shallowTraces[i].className )
         {
            strncpy( t.name, _shallowTraces[i].className, sizeof( t.name )-1 );
         }
         strncat( t.name, _shallowTraces[i].fctName, sizeof( t.name ) - strlen( t.name ) -1 );
      }

      _client.send( buffer, sizeof( MsgHeader ) + traceMsgSize );

      // Free the buffer
      _shallowTraces.clear();
      free( buffer );
   }

   int _pushTraceLevel{0};
   size_t _hashedThreadId{0};
   std::vector< ShallowTrace > _shallowTraces;
   Client _client;
   bool _isClientConnected{false};
};

ClientProfiler::Impl* ClientProfiler::Get( std::thread::id tId )
{
   static std::mutex m;
   std::lock_guard< std::mutex > guard(m);
   const size_t threadIdHash = std::hash<std::thread::id>{}( tId );

   int tIndex = 0;
   int firstEmptyIdx = -1;
   for ( ; tIndex < MAX_THREAD_NB; ++tIndex )
   {
      // Find existing client profiler for current thread.
      if ( threadsId[tIndex] == threadIdHash ) break;
      if ( firstEmptyIdx == -1 && threadsId[tIndex] == 0 ) firstEmptyIdx = tIndex;
   }

   // If we have not found any existing client profiler for the current thread,
   // lets create one.
   if ( tIndex >= MAX_THREAD_NB )
   {
      assert( firstEmptyIdx < MAX_THREAD_NB );
      // TODO: fix leak and fix potential condition race... -_-
      tIndex = firstEmptyIdx;
      threadsId[tIndex] = threadIdHash;
      clientProfilers[tIndex] = new ClientProfiler::Impl(threadIdHash);
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
   if( remainingPushedTraces <= 0 )
   {
      impl->flushToServer();
   }
}

} // end of namespace details
} // end of namespace vdbg

// ------ cdbg_client.cpp------------

#endif  // end VDBG_IMPLEMENTATION

#endif  // !defined(VDBG_ENABLED)

#endif  // VDBG_H_