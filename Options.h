#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <vector>
#include <cstdint>

namespace hop
{

struct Options
{
   float traceHeight{20.0f};
   bool startFullScreen{true};
   std::vector< uint32_t > threadColors;

   bool optionWindowOpened{false};
};

extern Options g_options;

bool saveOptions();
bool loadOptions();
void drawOptionsWindow( Options& opt );
uint32_t getColorForThread( const Options& opt, uint32_t threadIdx );
void setThreadCount( Options& opt, uint32_t threadCount );

}

#endif // OPTIONS_H_
