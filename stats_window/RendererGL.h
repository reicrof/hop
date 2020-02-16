#ifndef RENDERER_GL_H_
#define RENDERER_GL_H_

#include <cstdint>

struct ImDrawData;

namespace renderer
{

extern uint32_t g_FontTexture;

void setViewport( uint32_t x, uint32_t y, uint32_t width, uint32_t height );
void clearColorBuffer();
void renderDrawlist( ImDrawData* draw_data );
void createResources();
void setVSync( bool on );

} // namespace renderer

#endif // RENDERER_GL_H_