#ifndef PROFILER_H_
#define PROFILER_H_

#include "Hop.h"
#include "Timeline.h"
#include "ThreadInfo.h"
#include "StringDb.h"
#include "Server.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <future>

namespace hop
{

class Server;
struct Profiler
{
   Profiler( const char* name );
   ~Profiler();
   void update( float deltaTimeMs ) noexcept;
   void draw();
   void fetchClientData();
   void addTraces( const DisplayableTraces& traces, uint32_t threadIndex );
   void addStringData( const std::vector< char >& stringData, uint32_t threadIndex);
   void addLockWaits( const std::vector< LockWait >& lockWaits, uint32_t threadIndex);
   void addUnlockEvents(const std::vector<UnlockEvent>& unlockEvents, uint32_t threadIndex);
   void handleHotkey();
   bool setRecording( bool recording );
   void clear();

private:
   void drawMenuBar();
   void drawSearchWindow();
   void drawTraceDetailsWindow();
   bool openFile( const char* path );
   bool saveToFile( const char* path );
   void displayModalWindow();

   std::string _name;
   Timeline _timeline;
   std::vector< ThreadInfo > _tracesPerThread;
   StringDb _strDb;
   bool _recording{ false };
   bool _searchWindowOpen{ false };
   bool _focusSearchWindow{ false };

   const char* _errorModalWindowMsg{ nullptr };
   const char* _waitModalMessage{ nullptr };
   std::future< bool > _asyncJobDone;

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
void draw();

} // namespace hop

#endif  // PROFILER_H_
