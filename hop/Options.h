#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "Hop.h"

#include <array>
#include <cstdint>

namespace hop
{

namespace options
{
   bool save();
   bool load();
   void draw();
   void enableOptionWindow();

   float traceHeight();
   float traceTextAlignment();
   float windowOpacity();
   bool showDebugWindow();
   bool showCoreInfo();
   bool fullscreen();
   bool vsyncOn();
   const std::array< uint32_t, HOP_ZONE_MAX + 1 >& zoneColors();
} 

} // namespace hop

#endif // OPTIONS_H_
