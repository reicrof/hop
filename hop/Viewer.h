#ifndef VIEWER_H_
#define VIEWER_H_

#include "hop/Timeline.h"

#include <future>
#include <memory>
#include <vector>

namespace hop
{
class ProfilerView;
class Viewer
{
  public:
   Viewer( uint32_t screenSizeX, uint32_t screenSizeY );
   ~Viewer();
   int addNewProfiler( const char* processname, bool startRecording );
   void openProfilerFile();
   int removeProfiler( int index );
   int profilerCount() const;
   int activeProfilerIndex() const;
   bool fetchClientsData();

  const ProfilerView* getProfiler( int index ) const;
  ProfilerView* getProfiler( int index );

   void onNewFrame(
       float deltaMs,
       float width,
       float height,
       int mouseX,
       int mouseY,
       bool lmbPressed,
       bool rmbPressed,
       float mouseWheel );
   bool update(float deltaMs);
   bool draw( float windowWidth, float windowHeight );

   bool handleHotkey( ProfilerView* selectedProf );
   bool handleMouse( ProfilerView* selectedProf );

  private:
   Timeline _timeline;
   std::vector<std::unique_ptr<hop::ProfilerView> > _profilers;
   int _selectedTab;

   std::shared_future< ProfilerView* > _pendingProfilerLoad;

   bool _vsyncEnabled;
};

} // namespace hop

#endif  // HOP_VIEWER_H_