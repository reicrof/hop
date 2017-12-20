#include <vdbg_client.h>
#include <client.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <mutex>

namespace vdbg
{
namespace details
{
size_t ClientProfiler::threadsId[MAX_THREAD_NB] = {0};
ClientProfiler::Impl* ClientProfiler::clientProfilers[MAX_THREAD_NB] = {0};

class ClientProfiler::Impl
{
   struct ShallowTrace
   {
      const char *className, *fctName;
      vdbg::TimeStamp start, end;
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
       vdbg::TimeStamp start,
       vdbg::TimeStamp end,
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
          sizeof( vdbg::TracesInfo ) + sizeof( vdbg::Trace ) * _shallowTraces.size();
      uint8_t* buffer = (uint8_t*)malloc( sizeof( vdbg::MsgHeader ) + traceMsgSize );
      memset ( buffer, 0, sizeof( vdbg::MsgHeader ) + traceMsgSize );

      vdbg::MsgHeader* msgHeader = (vdbg::MsgHeader*)buffer;
      vdbg::TracesInfo* tracesInfo = (vdbg::TracesInfo*)(buffer + sizeof( vdbg::MsgHeader ) );
      vdbg::Trace* traceToSend =
          (vdbg::Trace*)( buffer + sizeof( vdbg::MsgHeader ) + sizeof( vdbg::TracesInfo ) );

      // Create the msg header first
      msgHeader->type = vdbg::MsgType::PROFILER_TRACE;
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

      _client.send( buffer, sizeof( vdbg::MsgHeader ) + traceMsgSize );

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
    vdbg::TimeStamp start,
    vdbg::TimeStamp end,
    uint8_t group )
{
   const int remainingPushedTraces = --impl->_pushTraceLevel;
   impl->addProfilingTrace( classStr, name, start, end, group );
   if( remainingPushedTraces <= 0 )
   {
      impl->flushToServer();
   }
}

}  // namespace details
}  // namespace vdbg
