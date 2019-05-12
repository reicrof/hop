#ifndef VIEWER_H_
#define VIEWER_H_

#include <chrono>
#include <memory>
#include <vector>

namespace hop
{
class Profiler;
class Viewer
{
  public:
   Viewer( uint32_t screenSizeX, uint32_t screenSizeY );
   ~Viewer();
   void addNewProfiler( const char* processname, bool startRecording );
   int removeProfiler( int index );
   Profiler* getProfiler( int index );
   int profilerCount() const;
   int activeProfilerIndex() const;
   void fetchClientsData();
   void onNewFrame(
       int width,
       int height,
       int mouseX,
       int mouseY,
       bool lmbPressed,
       bool rmbPressed,
       float mouseWheel );
   void draw( uint32_t windowWidth, uint32_t windowHeight );

   bool handleHotkey();

  private:
   std::vector<std::unique_ptr<hop::Profiler> > _profilers;
   int _selectedTab;

   using ClockType = std::chrono::steady_clock;
   std::chrono::time_point<ClockType> _lastFrameTime;
   bool _vsyncEnabled;
};

} // namespace hop

#endif  // HOP_VIEWER_H_