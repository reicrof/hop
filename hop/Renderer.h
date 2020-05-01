#ifndef HOP_RENDERER_H_
#define HOP_RENDERER_H_

struct SDL_Window;
struct ImDrawData;

namespace renderer
{

bool initialize( SDL_Window* window );
void terminate();
const char* sdlRenderDriverHint();
void renderDrawlist( ImDrawData* draw_data );
void setVSync( bool on );

} // namespace renderer

#endif // HOP_RENDERER_H_