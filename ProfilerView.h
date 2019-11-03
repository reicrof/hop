#ifndef PROFILER_H_
#define PROFILER_H_

#include "Timeline.h"
#include "common/Profiler.h"
#include "TraceSearch.h"
#include "StringDb.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace hop
{

struct ProfilerStats;

class ProfilerView
{
public:
   enum SourceType
   {
      SRC_TYPE_NONE,
      SRC_TYPE_FILE,
      SRC_TYPE_PROCESS,
   };

   Profiler();
   ~Profiler();
   ProfilerStats stats() const;

   void update( float deltaTimeMs, float globalTimeMs );
   void draw( float drawPosX, float drawPosY, float windowWidth, float windowHeight );

   void handleHotkey();
   void handleMouse();

private:
   Timeline _timeline;
   Profiler _profiler;
};


} // namespace hop

#endif  // PROFILER_H_
