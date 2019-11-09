#ifndef PROFILER_VIEW_H_
#define PROFILER_VIEW_H_

class Profiler;

#include "common/Profiler.h"

namespace hop
{

class ProfilerView
{
public:
   ProfilerView( Profiler::SourceType type, int processId, const char* str );
   void update( float deltaTimeMs, float globalTimeMs );
   void draw( float drawPosX, float drawPosY, float windowWidth, float windowHeight );

   void handleHotkey();
   bool handleMouse( float posX, float posY, bool lmClicked, bool rmClicked, float wheel );

   void setRecording( bool recording );

   const int lodLevel() const;
   const Profiler& data() const;
private:
   Profiler _profiler;
};

} // namespace hop

#endif  // PROFILER_VIEW_H_
