#ifndef IMDBG_H_
#define IMDBG_H_

#include "Hop.h"
#include "Timeline.h"
#include "ThreadInfo.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace hop
{

class Server;
struct Profiler
{
   Profiler( const std::string& name );
   ~Profiler();
   void update( float deltaTimeMs ) noexcept;
   void draw();
   void fetchClientData();
   void addTraces( const DisplayableTraces& traces, uint32_t threadId );
   void addStringData( const std::vector< char >& stringData, uint32_t threadId );
   void addLockWaits( const std::vector< LockWait >& lockWaits, uint32_t threadId );
   void handleHotkey();
   void setRecording( bool recording );

private:
   void drawMenuBar();

   std::string _name;
   Timeline _timeline;
   std::vector< uint32_t > _threadsId;
   std::vector< ThreadInfo > _tracesPerThread;
   bool _recording{ false };

   // Client/Server data
   // TODO: rethink and redo this part
   std::unique_ptr< Server > _server;
   std::vector< uint32_t > threadIds;
   std::vector< hop::DisplayableTraces > pendingTraces;
   std::vector< std::vector< char > > stringData;

   std::vector< uint32_t > threadIdsLockWaits;
   std::vector<std::vector< hop::LockWait > > pendingLockWaits;
};

// Initialize the imgui framework
void init();
// Add new profiler to be drawn
void addNewProfiler( Profiler* profiler );
// Updates the imgui data. Should be called each frame
void onNewFrame( int width, int height, int mouseX, int mouseY, bool lmbPressed, bool rmbPressed, float mouseWheel );
// Draw the ui
void draw();

} // namespace hop

#endif  // IMDBG_H_
