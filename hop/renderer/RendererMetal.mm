#include "hop/Renderer.h"

static id<MTLDevice> device;
static id<MTLCommandQueue> queue;
static const CAMetalLayer* swapchain;

namespace renderer
{

const char* sdlRenderDriverHint()
{
    return "metal";
}

void initialize()
{
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    swapchain              = (__bridge CAMetalLayer *)SDL_RenderGetMetalLayer(renderer);
    device                 = swapchain.device;
    queue                  = [device newCommandQueue];

    SDL_DestroyRenderer(renderer);
}

void renderDrawlist( ImDrawData* draw_data )
{
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
}

void setVSync( bool on )
{

}

} // namespace renderer