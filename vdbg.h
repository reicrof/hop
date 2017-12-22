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
#define VDBG_GET_THREAD_ID() std::this_thread::get_id()
extern char* __progname;
inline const char* getProgName() VDBG_NOEXCEPT
{
   return __progname;
}
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
inline decltype( std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count() ) getTimeStamp()
{
   return std::chrono::duration_cast<Precision>( Clock::now().time_since_epoch() ).count();
}
using TimeStamp = decltype( getTimeStamp() );

VDBG_CONSTEXPR uint32_t EXPECTED_TRACE_INFO_SIZE = 16;
struct TracesInfo
{
   uint32_t threadId;
   uint32_t stringDataSize;
   uint32_t traceCount;
   uint32_t padding;
};
VDBG_STATIC_ASSERT(
    sizeof( TracesInfo ) == EXPECTED_TRACE_INFO_SIZE,
    "TracesInfo layout has changed unexpectedly" );

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

#define VDBG_COMBINE( X, Y ) X##Y
#define VDBG_PROF_GUARD_VAR( LINE, ARGS ) \
   vdbg::details::ProfGuard VDBG_COMBINE( vdbgProfGuard, LINE ) ARGS

// -------- end of vdbg_client.h -------------

// ------ client.h ------------
class Client
{
  public:
   Client() VDBG_NOEXCEPT;
   ~Client();
   bool connect( bool force ) VDBG_NOEXCEPT;
   bool send( uint8_t* data, uint32_t size ) VDBG_NOEXCEPT;
   bool connected() const VDBG_NOEXCEPT;

  private:
   enum class State : uint8_t
   {
      NOT_CONNECTED = 0,
      CONNECTED,
      BROKEN_PIPE,
      ACCESS_ERROR,
      UNKNOWN_SHOULD_INVESTIGATE,
   };

   bool tryCreateSocket() VDBG_NOEXCEPT;
   void handleError() VDBG_NOEXCEPT;
   void closeSocket() VDBG_NOEXCEPT;

   int _socket{-1};
   State _state{State::NOT_CONNECTED};
   Clock::time_point _lastConnectionAttempt{};
   static VDBG_CONSTEXPR uint32_t connectionAttemptTimeoutInMs = 1000;
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
#include <algorithm>
#include <cassert>
#include <mutex>
#include <vector>

namespace vdbg
{
namespace details
{

static const char* SERVER_PATH = "/tmp/my_server";

// ------ client.cpp ------------

Client::Client() VDBG_NOEXCEPT
{
   tryCreateSocket();
}

bool Client::connect( bool force ) VDBG_NOEXCEPT
{
   using namespace std::chrono;
   auto now = Clock::now();
   if ( force || duration_cast<milliseconds>( now - _lastConnectionAttempt ).count() >
                     connectionAttemptTimeoutInMs )
   {
      if ( _socket == -1 )
      {
         if ( !tryCreateSocket() ) return false;
      }
      struct sockaddr_un serveraddr;
      memset( &serveraddr, 0, sizeof( serveraddr ) );
      serveraddr.sun_family = AF_UNIX;
      strcpy( serveraddr.sun_path, SERVER_PATH );

      int rc = ::connect( _socket, (struct sockaddr*)&serveraddr, SUN_LEN( &serveraddr ) );
      if ( rc < 0 )
      {
         // perror( "connect() failed" );
         handleError();
         _lastConnectionAttempt = now;
         return false;
      }

      _state = State::CONNECTED;
      return true;
   }
   return false;
}

bool Client::tryCreateSocket() VDBG_NOEXCEPT
{
   _socket = socket( AF_UNIX, SOCK_STREAM, 0 );
   if( _socket < 0 )
   {
      handleError();
      return false;
   }

   return true;
}

bool Client::send( uint8_t* data, uint32_t size ) VDBG_NOEXCEPT
{
   if( _state != State::CONNECTED && !connect( false ) ) return false;

   int rc = ::send( _socket, data, size, MSG_NOSIGNAL );
   if ( rc < 0 )
   {
      handleError();
      return false;
   }

   return true;
}

bool Client::connected() const VDBG_NOEXCEPT
{
   return _state == State::CONNECTED;
}

void Client::closeSocket() VDBG_NOEXCEPT
{
   if ( _socket != -1 )
   {
      ::close( _socket );
      _socket = -1;
   }
}

void Client::handleError() VDBG_NOEXCEPT
{
   switch ( errno )
   {
      case ENOENT:
         break;
      case EPIPE:
         _state = State::BROKEN_PIPE;
         closeSocket();
         break;
      case EACCES:
         _state = State::ACCESS_ERROR;
         closeSocket();
         break;
      default:
         perror( "send() falied with unhandled error" );
         _state = State::UNKNOWN_SHOULD_INVESTIGATE;
         closeSocket();
         break;
   }
}

Client::~Client()
{
   closeSocket();
}

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
      _nameArrayId.reserve( 64 );
      _nameArrayData.reserve( 64 * 32 );

      // Push back first name as empty string
      _nameArrayData.push_back('\0');
      _nameArrayId.push_back(NULL);

      _client.connect( true );
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
      if( !_client.connected() )
      {
         if( !_client.connect( false ) )
         {
            // Cannot connect to server
            _shallowTraces.clear();
            return;
         }
      }

      std::vector< std::pair< uint32_t, uint32_t > > classFctNamesIdx;
      classFctNamesIdx.reserve( _shallowTraces.size() );
      for( const auto& t : _shallowTraces  )
      {
         classFctNamesIdx.emplace_back(
             findOrAddStringToDb( t.className ), findOrAddStringToDb( t.fctName ) );
      }

      // Allocate raw buffer to send to server. This raw data should be as follow:
      // =========================================================
      // msgHeader   = Generic Msg Header       - Generic msg information
      // tracesInfo  = Profiler specific Header - Information about the profiler specific msg
      // stringData  = String Data              - Array with all strings referenced by the traces
      // traceToSend = Traces                   - Array containing all of the traces
      const uint32_t stringDataSize = _nameArrayData.size();
      const size_t profilerMsgSize =
          sizeof( TracesInfo ) + stringDataSize + sizeof( Trace ) * _shallowTraces.size();
      uint8_t* buffer = (uint8_t*)malloc( sizeof( MsgHeader ) + profilerMsgSize );
      memset ( buffer, 0, sizeof( MsgHeader ) + profilerMsgSize );

      MsgHeader* msgHeader = (MsgHeader*)buffer;
      TracesInfo* tracesInfo = (TracesInfo*)(buffer + sizeof( MsgHeader ) );
      char* stringData = (char*)( buffer + sizeof( MsgHeader ) + sizeof( TracesInfo ) );
      Trace* traceToSend =
          (Trace*)( buffer + sizeof( MsgHeader ) + sizeof( TracesInfo ) + stringDataSize );

      // Create the msg header first
      msgHeader->type = MsgType::PROFILER_TRACE;
      msgHeader->size = profilerMsgSize;

      // TODO: Investigate if the truncation from size_t to uint32 is safe .. or not
      tracesInfo->threadId = (uint32_t)_hashedThreadId;
      tracesInfo->stringDataSize = stringDataSize;
      tracesInfo->traceCount = (uint32_t)_shallowTraces.size();

      // Copy string data into its array
      memcpy( stringData, _nameArrayData.data(), stringDataSize );

      // Copy trace information into buffer to send
      for( size_t i = 0; i < _shallowTraces.size(); ++i )
      {
         auto& t = traceToSend[i];
         t.start = _shallowTraces[i].start;
         t.end = _shallowTraces[i].end;
         t.classNameIdx = classFctNamesIdx[i].first;
         t.fctNameIdx = classFctNamesIdx[i].second;
         t.group = _shallowTraces[i].group;
      }

      _client.send( buffer, sizeof( MsgHeader ) + profilerMsgSize );

      // Free the buffer
      _shallowTraces.clear();
      free( buffer );
   }

   int _pushTraceLevel{0};
   size_t _hashedThreadId{0};
   std::vector< ShallowTrace > _shallowTraces;
   std::vector< const char* > _nameArrayId;
   std::vector< char > _nameArrayData;
   Client _client;
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