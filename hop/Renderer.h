#ifndef HOP_RENDERER_H_
#define HOP_RENDERER_H_

struct ImDrawData;

namespace renderer
{

void initialize();
const char* sdlRenderDriverHint();
void renderDrawlist( ImDrawData* draw_data );
void setVSync( bool on );

} // namespace renderer

#endif // HOP_RENDERER_H_