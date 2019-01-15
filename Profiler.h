#ifndef PROFILER_H_
#define PROFILER_H_

#include "Hop.h"
#include "Timeline.h"
#include "TimelineTrack.h"
#include "TraceSearch.h"
#include "StringDb.h"
#include "Server.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace hop
{
class Server;
struct Profiler
{
   Profiler( const char* name );
   ~Profiler();
   void update( float deltaTimeMs ) noexcept;
   void draw( uint32_t windowWidth, uint32_t windowHeight );
   void fetchClientData();
   void addStringData( const std::vector< char >& stringData );
   void addTraces( const TraceData& traces, uint32_t threadIndex );
   void addLockWaits( const LockWaitData& lockWaits, uint32_t threadIndex);
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex);
   void addCoreEvents( const std::vector<CoreEvent>& coreEvents, uint32_t threadIndex );
   void addThreadName( StrPtr_t name, uint32_t threadIndex );
   void handleHotkey( bool modalWindowOpened );
   void handleMouse();
   void setRecording( bool recording );
   void clear();

private:
   void drawMenuBar();
   bool openFile( const char* path );
   bool saveToFile( const char* path );

   std::string _name;
   Timeline _timeline;
   TimelineTracks _tracks;
   StringDb _strDb;
   bool _recording{ false };

   Server _server;
   Server::PendingData _serverPendingData;
};

// Initialize the imgui framework
void init();
// Add new profiler to be drawn
void addNewProfiler( Profiler* profiler );
// Updates the imgui data. Should be called each frame
void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
// Draw the ui
void draw( uint32_t windowWidth, uint32_t windowHeight );

} // namespace hop

#endif  // PROFILER_H_
