#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "Hop.h"

#include <array>
#include <cstdint>

namespace hop
{

struct Options
{
   float traceHeight{20.0f};
   bool startFullScreen{true};
   bool vsyncOn{true};
   bool glFinishByDefault{false};
   bool debugWindow{false};
   std::array< uint32_t, HOP_MAX_ZONES + 1 > zoneColors;
   std::array< bool, HOP_MAX_ZONES + 1 > zoneEnabled;
   float disabledZoneOpacity{0.2f};

   bool optionWindowOpened{false};
};

extern Options g_options;

bool saveOptions();
bool loadOptions();
void drawOptionsWindow( Options& opt );

}

#endif // OPTIONS_H_
