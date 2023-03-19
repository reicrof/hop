#include "common/Server.h"
#include "common/Utils.h"
#include "common/platform/Platform.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <algorithm>
#include <chrono>

static constexpr int POLL_COUNT_FAIL_DISCONNECTION = 20;

template <typename T, class BinaryPredicate, class MergeFct>
static T merge_consecutive( T first, T last, BinaryPredicate pred, MergeFct merge )
{
   int mergeCount = 0;
   auto writePos = first;
   while ( ++first != last )
   {
      if ( !pred( *writePos, *first ) )
      {
         merge( *( first - 1 ), *writePos );
         std::swap( *( first - 1 ), *writePos );
         writePos = first;
         ++mergeCount;
      }
   }
   merge( *( first - 1 ), *writePos );
   std::swap( *( first - 1 ), *writePos );

   return last - mergeCount;
}

static int mergeAndRemoveDuplicates( hop::CoreEvent* coreEvents, uint32_t count, float cpuGHz )
{
   // Merge events that are less than 10 micro apart
   const uint64_t minCycles = hop::nanosToCycles( 10000, cpuGHz );
   auto canMergeCore = [minCycles]( const hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) {
      return lhs.core == rhs.core &&
             ( ( rhs.start < lhs.end || ( rhs.start - lhs.end ) < minCycles ) );
   };

   auto sameCore = []( const hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) {
      return lhs.core == rhs.core;
   };

   auto mergedEnd = merge_consecutive(
       coreEvents,
       coreEvents + count,
       canMergeCore,
       []( hop::CoreEvent& lhs, const hop::CoreEvent& rhs ) { lhs.start = rhs.start; } );
   auto newEnd = std::unique( coreEvents, mergedEnd, sameCore );
   return std::distance( coreEvents, newEnd );
}

static hop::ConnectionState updateConnectionState(
    const hop::SharedMemory& sharedMem,
    int pollFailCount )
{
   hop::ConnectionState state = hop::CONNECTED;
   const bool producerLost =
       !sharedMem.hasConnectedProducer() || pollFailCount > POLL_COUNT_FAIL_DISCONNECTION;
   if( producerLost )
   {
      // We have lost the producer. Check if it is becaused the process was terminated or
      // if it is simply not feeling chatty
      state = hop::processAlive( sharedMem.pid() ) ? hop::CONNECTED_NO_CLIENT
                                                   : hop::NOT_CONNECTED;
   }
   return state;
}

static uint16_t getShortNameIndex( const std::string longname )
{
   uint16_t shortnameIdx = 0;
   size_t shortNamePos = longname.find_last_of("/\\") + 1;
   if (shortNamePos != std::string::npos)
      shortnameIdx = (uint16_t)shortNamePos;
   return shortnameIdx;
}

namespace hop
{
class Transport
{
  public:
   Transport() = default;
#if HOP_USE_REMOTE_PROFILER
   Transport( NetworkConnection* nc ) : _network( nc ) {}
#endif

   bool create( int /* pid */, const char* /* name */ ) { return true; }

   void setConsumerState(int connected, int listening)
   {
      if( _shmem.valid () )
      {
          if( connected >= 0 )
            _shmem.setConnectedConsumer( connected );
          if( listening >= 0 )
            _shmem.setListeningConsumer( listening );
      }
#if HOP_USE_REMOTE_PROFILER
      if( _network && _network->status() == NetworkConnection::Status::ALIVE )
      {
         ViewerMsgInfo info = {};
         info.connected = connected;
         info.listening = listening;
         info.requestHandshake = !connected || !listening;
         _network->sendAllData( &info, sizeof( info ), false );
      }
#endif
   }

   void setResetSeed( uint32_t seed )
   {
      if( _shmem.valid() ) _shmem.setResetSeed( seed );

#if HOP_USE_REMOTE_PROFILER
      if( _network && _network->status() == NetworkConnection::Status::ALIVE )
      {
         ViewerMsgInfo info  = {};
         info.seed      = seed;
         _network->sendAllData( &info, sizeof( info ), false );
      }
#endif
   }

   void requestHandshake()
   {
#if HOP_USE_REMOTE_PROFILER
      if( _network && _network->status() == NetworkConnection::Status::ALIVE )
      {
         ViewerMsgInfo info    = {};
         info.requestHandshake = true;
         _network->sendAllData( &info, sizeof( info ), false );
      }
#endif
   }

   SharedMemory _shmem;
#if HOP_USE_REMOTE_PROFILER
   NetworkConnection *_network;
   std::atomic<bool> _networkThreadReady;
#endif
};

static void shmemTransportLoop( Server* server, Transport *transport, int pid )
{
   int pollFailedCount                 = 0;
   ConnectionState prevConnectionState = NOT_CONNECTED;

   uint32_t reconnectTimeoutMs = 10;
   uint32_t seed          = 0;
   while( true )
   {
      // Try to get the shared memory
      if( !transport->_shmem.valid() )
      {
         if( server->tryConnect( pid, prevConnectionState ) )
         {
            // Sleep few ms before retrying. Increase timeout time each try
            hop::sleepMs( reconnectTimeoutMs );
            static constexpr uint32_t MAX_RECONNECT_TIMEOUT_MS = 500;
            reconnectTimeoutMs = std::min( reconnectTimeoutMs + 10, MAX_RECONNECT_TIMEOUT_MS );
            continue;  // No connection, let's retry
         }

         if( prevConnectionState != CONNECTED )
            break;  // No connection was found and we should not retry.

         printf( "Connection to shared data successful.\n" );
         std::lock_guard<hop::Mutex> guard( server->_stateMutex );
         seed = server->_state.seed = transport->_shmem.lastResetSeed();
      }

      HOP_PROF_FUNC();

      // Check if its been a while since we have been signaleds
      const auto newConnectionState = updateConnectionState( transport->_shmem, pollFailedCount );
      const bool clientAlive        = newConnectionState != NOT_CONNECTED;

      // Check and update current server state
      {
         std::lock_guard<hop::Mutex> guard( server->_stateMutex );

         //  Stop processing if we are done running
         if( !server->_state.running ) break;

         // A clear was requested so we need to clear our string database
         if( server->_state.seed > seed )
         {
            server->_stringDb.clear();
            server->clearPendingMessages();
            server->_threadNamesReceived.clear();
            transport->_shmem.setResetSeed( server->_state.seed );
            seed = server->_state.seed;
            continue;
         }

         // Update state if it has changed
         if( prevConnectionState != newConnectionState )
         {
            prevConnectionState    = newConnectionState;
            server->_state.connectionState = newConnectionState;
         }

         if( !clientAlive )
         {
            server->_state.pid = -1;
            transport->_shmem.destroy();
            continue;
         }
      }

      size_t offset            = 0;
      const size_t bytesToRead = ringbuf_consume( transport->_shmem.ringbuffer(), &offset );
      if( bytesToRead > 0 )
      {
         HOP_PROF( "Server - Handling new messages" );
         pollFailedCount              = 0;
         size_t bytesRead             = 0;
         while( bytesRead < bytesToRead )
         {
            size_t bread = server->handleNewMessage(
                transport->_shmem.data() + offset + bytesRead, bytesToRead - bytesRead, seed );
            if( bread <= 0 ) break;

            bytesRead += bread;
         }
         ringbuf_release( server->_transport->_shmem.ringbuffer(), bytesToRead );
      }
      else
      {
         // Nothing was sent
         ++pollFailedCount;
         HOP_PROF( "Nothing send..." );

         // If we have a connected producer and we timed out, it simply means the app is either
         // not very productive or is being debuged. If we do not have a producer, it means the
         // app was closed.
         using namespace std::chrono;
         if( clientAlive )
         {
            // Relax our polling after a few attemps
            hop::sleepMs( 1 * std::min( pollFailedCount, 10 ) );
         }
         else
         {
            hop::sleepMs( 500 );
         }
      }
   }
}

#if HOP_USE_REMOTE_PROFILER
static uint32_t processUncompressData( Server* server, uint32_t curSeed, const uint8_t *data, ssize_t size )
{
    ssize_t bytesRead = 0;
    while( bytesRead < size )
    {
       uint32_t newSeed = curSeed;
       size_t bread =
           server->handleNewMessage( data + bytesRead, size - bytesRead, newSeed );
       if( bread <= 0 ) break;

       bytesRead += bread;

       // If we have received a new handshake, the seed might have been changed. We need to act
       // on it now, otherwise we could skip some incoming string data
       if( newSeed > curSeed )
       {
          curSeed = newSeed;
          std::lock_guard<hop::Mutex> guard( server->_stateMutex );
          server->_stringDb.clear();
          server->_threadNamesReceived.clear();
       }
    }
    assert( bytesRead == size);
    return curSeed;
}
static void
networkTransportLoop( Server* server, Transport* transport, NetworkConnection* nc )
{
   const uint32_t bufSize = 1024 * 1024 * 8;
   uint8_t* buf           = (uint8_t*)malloc( bufSize );
   uint8_t* compBuf       = (uint8_t*)malloc( bufSize );

   transport->requestHandshake();
   transport->_networkThreadReady.store( true );

   uint32_t curSeed = 0;
   uint32_t curDataSize = 0;
   while( true )
   {
      // Check and update current server state
      {
         std::lock_guard<hop::Mutex> guard( server->_stateMutex );
         //  Stop processing if we are done running
         if( !server->_state.running ) break;

         if( server->_state.seed > curSeed )
         {
            server->_stringDb.clear();
            server->_threadNamesReceived.clear();
            transport->setResetSeed( server->_state.seed );
            curSeed = server->_state.seed;
            continue;
         }
      }

      const ssize_t recSize = nc->receiveData( compBuf + curDataSize, bufSize - curDataSize );
      if( recSize > 0 )
         curDataSize += recSize;

      NetworkCompressionHeader compressionHeader = {};
      if( curDataSize >= sizeof( NetworkCompressionHeader ) )
      {
         compressionHeader = *(NetworkCompressionHeader*)( compBuf );
         assert( compressionHeader.canary == 0xbadc0ffe );
      }

      uint8_t* compBufIt = compBuf;
      while( curDataSize > compressionHeader.compressedSize + sizeof( compressionHeader ) )
      {
         const size_t cmpSize  = compressionHeader.compressedSize;
         const size_t ucmpSize = compressionHeader.uncompressedSize;
         compBufIt += sizeof( compressionHeader );

         const uint8_t* uncompressedData = compBufIt;
         if( compressionHeader.compressed )
         {
            int decomp_res = lzjb_decompress( compBufIt, buf, cmpSize, ucmpSize );
            assert( decomp_res >= 0 );
            uncompressedData = buf;
         }

         curSeed = processUncompressData( server, curSeed, uncompressedData, ucmpSize );

         assert( curDataSize > cmpSize + sizeof( compressionHeader ) );
         curDataSize -= cmpSize + sizeof( compressionHeader );
         compBufIt += cmpSize;

         /* Fetch next compression header */
         if( curDataSize >= sizeof( compressionHeader ) )
         {
            memcpy( &compressionHeader, compBufIt, sizeof( NetworkCompressionHeader ) );
            assert( compressionHeader.canary == 0xbadc0ffe );
         }
      }

      if( curDataSize && compBuf != compBufIt )
         memmove( compBuf, compBufIt, curDataSize );
   }

   free( buf );
   free( compBuf );
}
#endif

bool Server::start( int inPid, const char* name )
{
   assert( name != nullptr );

   _state.running = true;
   _state.pid = -1;
   _state.processName = name;
   _state.shortNameIndex = getShortNameIndex( _state.processName );
   _state.connectionState = NOT_CONNECTED;

   _transport = new Transport();

   _thread = std::thread( shmemTransportLoop, this, _transport, inPid );

   return true;
}

#if HOP_USE_REMOTE_PROFILER
bool Server::start( NetworkConnection& nc )
{
   _state.running         = true;
   _state.pid             = -1;
   _state.connectionState = NOT_CONNECTED;

   _transport = new Transport(&nc);

   _thread = std::thread( networkTransportLoop, this, _transport, &nc );
   while( !_transport->_networkThreadReady.load() )
   {
      hop::sleepMs( 100 );
   }

   return true;
}

const NetworkConnection* Server::networkConnection() const
{
   if( _transport ) return _transport->_network;
   return nullptr;
}
#endif

bool Server::tryConnect( int32_t pid, ConnectionState& newState )
{
   HOP_PROF_FUNC();

   hop::ProcessInfo procInfo = {-1, {}};
   if( pid != -1 )
      procInfo = hop::getProcessInfoFromPID( pid );
   else
   {
      hop::ProcessesInfo infos = hop::getProcessInfoFromProcessName( _state.processName.c_str() );
      if( infos.count > 0 )
      {
         procInfo = infos.infos[0];
         if( infos.count > 1 )
         {
            std::string msg;
            msg.reserve (512);
            msg += "Ambiguous process name\n";
            char buffer[512];
            size_t minProcSize = strlen( infos.infos[0].name );
            size_t minProcSizeIdx = 0;
            for( int i = 0; i < infos.count; i++ )
            {
               size_t len = strlen( infos.infos[i].name );
               if ( len < minProcSize)
               {
                  minProcSize = len;
                  minProcSizeIdx = i;
               }
               snprintf (buffer, sizeof( buffer ), "  [%lld] %s\n", infos.infos[i].pid, infos.infos[i].name);
               msg += buffer;
            }
            snprintf (buffer, sizeof( buffer ), "Arbitrarily choosing pid %lld\n", infos.infos[minProcSizeIdx].pid);
            msg += buffer;
            fprintf( stderr, "%s\n", msg.c_str() );
            procInfo = infos.infos[minProcSizeIdx];
         }
      }
   }

   newState = _transport->_shmem.create(procInfo.pid, 0 /*will be define in shared metadata*/, true);

   if( newState != CONNECTED )
   {
      {  // Update state then go to sleep a few MS
         std::lock_guard<hop::Mutex> guard( _stateMutex );
         _state.connectionState = newState;
         if( !_state.running ) return false;  // We are done without even opening the shared mem :(
      }

      // Signal we should retry
      return true;
   }

   // Clear any remaining messages from previous execution now
   clearPendingMessages();

   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _transport->setConsumerState( 1, _state.recording );

    _state.connectionState = newState;
   _state.pid             = procInfo.pid;
   _state.processName     = std::string( procInfo.name );
   _state.shortNameIndex  = getShortNameIndex( _state.processName );

   // Set HOP thread name
   char serverName[64];
   snprintf( serverName, sizeof( serverName ), "%s [Producer]", _state.processName.c_str() + _state.shortNameIndex );
   HOP_SET_THREAD_NAME( serverName );

   // Got the connection, signal that we should to retry
   return false;
}

const char* Server::processInfo( int* processId ) const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   static const char* noProcessStr = "<No Process>";
   if( processId ) *processId = _state.pid;
   return _state.processName.empty() ? noProcessStr : _state.processName.c_str();
}

const char* Server::shortProcessInfo( int* processId ) const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   static const char* noProcessStr = "<No Process>";
   if( processId ) *processId = _state.pid;
   return _state.processName.empty() ? noProcessStr : _state.processName.c_str() + _state.shortNameIndex;
}

ConnectionState Server::connectionState() const
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   return _state.connectionState;
}

size_t Server::sharedMemorySize() const
{
   if (_transport && _transport->_shmem.valid() )
   {
      return _transport->_shmem.sharedMetaInfo()->requestedSize;
   }

   return 0;
}

float Server::cpuFreqGHz() const
{
   if( _cpuFreqGHz == 0 && _transport && _transport->_shmem.valid() )
   {
      if( _transport->_shmem.sharedMetaInfo()->usingStdChronoTimeStamps )
      {
         // Using std::chrono means we are already using nanoseconds -> 1Ghz
         _cpuFreqGHz = 1.0f;
      }
      else
      {
         _cpuFreqGHz = hop::getCpuFreqGHz();
      }
   }

   return _cpuFreqGHz;
}

void Server::setRecording( bool recording )
{
   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _state.recording = recording;
   if (_transport)
      _transport->setConsumerState( 1, recording );
}

void Server::getPendingData( PendingData& data )
{
   HOP_PROF_FUNC();
   std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
   _sharedPendingData.swap( data );
   _sharedPendingData.clear();
}

bool Server::addUniqueThreadName( uint32_t threadIndex, StrPtr_t name )
{
   bool newInsert = false;
   if ( _threadNamesReceived.size() <= threadIndex )
   {
      _threadNamesReceived.resize( threadIndex + 1, 0 );
   }

   if ( _threadNamesReceived[threadIndex] == 0 )
   {
      _threadNamesReceived[threadIndex] = name;
      newInsert = true;
   }

   return newInsert;
}

static size_t msgSize (const MsgInfo *msgInfo)
{
    size_t size = sizeof(MsgInfo);
    switch ( msgInfo->type )
    {
        case MsgType::PROFILER_HEARTBEAT:
            break;
        case MsgType::PROFILER_STRING_DATA:
            size += msgInfo->stringData.size;
            assert (size % 8 == 0);
            break;
        case MsgType::PROFILER_TRACE:
        {
            constexpr size_t trace_size = ( sizeof( TimeStamp ) + sizeof( TimeStamp ) + sizeof( Depth_t ) +
                                           sizeof( StrPtr_t ) + sizeof( StrPtr_t ) + sizeof( LineNb_t ) +
                                           sizeof( ZoneId_t ) );
            size += trace_size * msgInfo->traces.count;
            break;
        }
        case MsgType::PROFILER_WAIT_LOCK:
            size += msgInfo->lockwaits.count * sizeof( LockWait );
            break;
        case MsgType::PROFILER_UNLOCK_EVENT:
            size += msgInfo->unlockEvents.count * sizeof( UnlockEvent );
            break;
        case MsgType::PROFILER_CORE_EVENT:
            size += msgInfo->coreEvents.count * sizeof( CoreEvent );
            break;
        case MsgType::PROFILER_HANDSHAKE:
            size += sizeof (NetworkHandshake);
            break;
        default:
            assert( false );

    }

    return size;
}

ssize_t Server::handleNewMessage( const uint8_t* data, size_t maxSize, uint32_t &seed )
{
   // Do we even have enough data to make sense of the headers
   if( maxSize < sizeof( MsgInfo ) ) return 0;

   const uint8_t* bufPtr = data;
   const MsgInfo* msgInfo = (const MsgInfo*)bufPtr;
   const MsgType msgType = msgInfo->type;
   const uint32_t threadIndex = msgInfo->threadIndex;

   assert ((uint32_t)msgInfo->type < (uint32_t)MsgType::INVALID_MESSAGE);
   bufPtr += sizeof( MsgInfo );

   const size_t totalMsgSize = msgSize (msgInfo);

   // If the message was sent from a previous reset seed ignore it, unless
   // it is a handshake message
   if( msgInfo->seed != seed && msgType != MsgType::PROFILER_HANDSHAKE ) return totalMsgSize;

   // If the whole message is not contain in the buffer, bail out here.
   if( totalMsgSize > maxSize ) return 0;

   // If the thread has an assigned name
   if ( msgInfo->threadName != 0 && addUniqueThreadName( threadIndex, msgInfo->threadName ) )
   {
      std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
      _sharedPendingData.threadNames.emplace_back( threadIndex, msgInfo->threadName );
    }

    switch ( msgType )
    {
       case MsgType::PROFILER_HEARTBEAT:
         break;
       case MsgType::PROFILER_STRING_DATA:
       {
          // Copy string and add it to database
          const size_t strSize = msgInfo->stringData.size;
          if ( strSize > 0 )
          {
             const char* strDataPtr = (const char*)bufPtr;
             bufPtr += strSize;
             assert (bufPtr - data <= maxSize);

             // TODO: Could lock later when we received all the messages
             std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
             _stringDb.addStringData( strDataPtr, strSize );
             _sharedPendingData.stringData.insert(
                 _sharedPendingData.stringData.end(), strDataPtr, strDataPtr + strSize );
          }
          break;
       }
       case MsgType::PROFILER_TRACE:
       {
          const size_t tracesCount = msgInfo->traces.count;
          if ( tracesCount > 0 )
          {
             TraceData traceData;
             const TimeStamp* starts = (const TimeStamp*)bufPtr;
             const TimeStamp* ends = starts + tracesCount;
             const StrPtr_t* fileNames = (const StrPtr_t*)( ends + tracesCount );
             const StrPtr_t* fctNames = fileNames + tracesCount;
             const LineNb_t* lineNbs = (const LineNb_t*)( fctNames + tracesCount );
             const Depth_t* depths     = (const Depth_t*)( lineNbs + tracesCount );
             const ZoneId_t* zones     = (const ZoneId_t*)( depths + tracesCount );

             HOP_CONSTEXPR size_t trace_size =
                 ( sizeof( TimeStamp ) + sizeof( TimeStamp ) + sizeof( Depth_t ) +
                   sizeof( StrPtr_t ) + sizeof( StrPtr_t ) + sizeof( LineNb_t ) +
                   sizeof( ZoneId_t ) );

             bufPtr += trace_size * tracesCount;
             assert( bufPtr - data <= maxSize );

             traceData.entries.ends.append( ends, ends + tracesCount );
             traceData.entries.starts.append( starts, starts + tracesCount );
             traceData.entries.depths.append( depths, depths + tracesCount );
             traceData.entries.maxDepth = *std::max_element( depths, depths + tracesCount );

             traceData.lineNbs.append( lineNbs, lineNbs + tracesCount );
             traceData.zones.append( zones, zones + tracesCount );

             // Process non trivially copiable members
             for ( size_t i = 0; i < tracesCount; ++i )
             {
                traceData.fileNameIds.push_back( _stringDb.getStringIndex( fileNames[i] ) );
                traceData.fctNameIds.push_back( _stringDb.getStringIndex( fctNames[i] ) );
             }

             // The ends time should already be sorted
             assert_is_sorted( traceData.entries.ends.begin(), traceData.entries.ends.end() );

             static_assert(
                 std::is_move_constructible<TraceData>::value, "Trace Data not moveable" );
             // TODO: Could lock later when we received all the messages
             std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
             _sharedPendingData.tracesPerThread[threadIndex].append( traceData );
          }
          break;
       }
      case MsgType::PROFILER_WAIT_LOCK:
      {
         const LockWait* lws = (const LockWait*)bufPtr;
         const uint32_t lwCount = msgInfo->lockwaits.count;

         LockWaitData lockwaitData;
         Depth_t maxDepth = 0;
         for ( uint32_t i = 0; i < lwCount; ++i )
         {
            lockwaitData.entries.ends.push_back( lws[i].end );
            lockwaitData.entries.starts.push_back( lws[i].start );
            lockwaitData.entries.depths.push_back( lws[i].depth );
            lockwaitData.mutexAddrs.push_back( lws[i].mutexAddress );
            maxDepth = std::max( maxDepth, lws[i].depth );
         }
         lockwaitData.entries.maxDepth = maxDepth;

         bufPtr += ( lwCount * sizeof( LockWait ) );
         assert (bufPtr - data <= maxSize);

         // The ends time should already be sorted
         assert_is_sorted( lockwaitData.entries.ends.begin(), lockwaitData.entries.ends.end() );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.lockWaitsPerThread[threadIndex].append( lockwaitData );
         break;
      }
      case MsgType::PROFILER_UNLOCK_EVENT:
      {
         const size_t eventCount = msgInfo->unlockEvents.count;
         UnlockEvent* eventPtr = (UnlockEvent*)bufPtr;

         bufPtr += eventCount * sizeof( UnlockEvent );
         assert (bufPtr - data <= maxSize);

         std::sort(
             eventPtr, eventPtr + eventCount, []( const UnlockEvent& lhs, const UnlockEvent& rhs ) {
                return lhs.time < rhs.time;
             } );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         auto& unlocks = _sharedPendingData.unlockEventsPerThread[threadIndex];
         unlocks.insert( unlocks.end(), eventPtr, eventPtr + eventCount );

         break;
      }
      case MsgType::PROFILER_CORE_EVENT:
      {
         const size_t eventCount = msgInfo->coreEvents.count;
         CoreEvent* coreEventsPtr = (CoreEvent*)bufPtr;

         // Must be done before removing duplicates
         bufPtr += eventCount * sizeof( CoreEvent );
         assert (bufPtr - data <= maxSize);
         assert (eventCount > 0);

         const size_t newCount = mergeAndRemoveDuplicates( coreEventsPtr, eventCount, cpuFreqGHz() );

         CoreEventData coresData;
         for ( uint32_t i = 0; i < newCount; ++i )
         {
            coresData.entries.ends.push_back( coreEventsPtr[i].end );
            coresData.entries.starts.push_back( coreEventsPtr[i].start );
            coresData.cores.push_back( coreEventsPtr[i].core );
         }
         coresData.entries.depths.append( newCount, 0 );

         // TODO: Could lock later when we received all the messages
         std::lock_guard<hop::Mutex> guard( _sharedPendingDataMutex );
         _sharedPendingData.coreEventsPerThread[threadIndex].append( coresData );
         break;
      }
      case MsgType::PROFILER_HANDSHAKE:
      {
         static_assert( offsetof( NetworkHandshakeMsgInfo, handshake ) == sizeof( MsgInfo ), "Struct alignment changed" );
         const NetworkHandshake* handshakePtr = (const NetworkHandshake*)bufPtr;
         std::lock_guard<hop::Mutex> guard( _stateMutex );
         _cpuFreqGHz = handshakePtr->cpuFreqGhz;
         _state.processName = handshakePtr->appName;
         _state.pid         = handshakePtr->pid;
         _state.seed        = msgInfo->seed;
         seed               = msgInfo->seed;
         bufPtr += sizeof( *handshakePtr );
         break;
      }
      default:
         assert( false );
   }

    assert (totalMsgSize == bufPtr - data);
   return ( size_t )( bufPtr - data );
}

void Server::clearPendingMessages()
{
   size_t offset = 0;
   while ( size_t bytesToRead = ringbuf_consume( _transport->_shmem.ringbuffer(), &offset ) )
   {
      ringbuf_release( _transport->_shmem.ringbuffer(), bytesToRead );
   }
}

void Server::clear()
{
   setRecording( false );

   std::lock_guard<hop::Mutex> guard( _stateMutex );
   _state.seed++;
}

void Server::stop()
{
   HOP_PROF_FUNC();
   bool wasRunning = false;
   {
      std::lock_guard<hop::Mutex> guard( _stateMutex );
      if( _state.running )
      {
         wasRunning     = true;
         _state.running = false;
      }
   }

   if( wasRunning )
   {
      uint32_t seed;
      {
         /* Set the new seed before shutting down the connection */
         std::lock_guard<hop::Mutex> guard( _stateMutex );
         seed = ++_state.seed;
      }
      _transport->setResetSeed( seed );
      _transport->setConsumerState(0, 0);

      if( _thread.joinable() )
      {
         _thread.join();
      }
   }
}

void Server::PendingData::clear()
{
   HOP_PROF_FUNC();

   stringData.clear();
   for ( auto& traces : tracesPerThread )
   {
      traces.second.clear();
   }

   for ( auto& lockwaits : lockWaitsPerThread )
   {
      lockwaits.second.clear();
   }

   for ( auto& unlockEvents : unlockEventsPerThread )
   {
      unlockEvents.second.clear();
   }

   for ( auto& coreEvents : coreEventsPerThread )
   {
      coreEvents.second.clear();
   }

   threadNames.clear();
}

void Server::PendingData::swap( PendingData& rhs )
{
   HOP_PROF_FUNC();
   using std::swap;
   swap( tracesPerThread, rhs.tracesPerThread );
   swap( stringData, rhs.stringData );
   swap( lockWaitsPerThread, rhs.lockWaitsPerThread );
   swap( unlockEventsPerThread, rhs.unlockEventsPerThread );
   swap( coreEventsPerThread, rhs.coreEventsPerThread );
   swap( threadNames, rhs.threadNames );
}

}  // namespace hop
