#ifndef OPTIONS_H_
#define OPTIONS_H_

namespace hop
{

struct Options
{
   float traceHeight{20.0f};
   bool startFullScreen{true};

   bool optionWindowOpened{false};
};

extern Options g_options;

bool saveOptions();
bool loadOptions();
void drawOptionsWindow( Options& opt );

}

#endif // OPTIONS_H_
