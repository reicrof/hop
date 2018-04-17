#ifndef OPTIONS_H_
#define OPTIONS_H_

namespace hop
{

struct Options
{
   bool startFullScreen{true};
};

extern Options g_options;

bool saveOptions();
bool loadOptions();

}

#endif // OPTIONS_H_
