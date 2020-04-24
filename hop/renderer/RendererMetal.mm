#include "hop/Renderer.h"

#include "imgui/imgui.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL.h>

static SDL_Renderer* g_renderer;
static id<MTLDevice> g_device;
static id<MTLCommandQueue> g_queue;
static id<MTLTexture> g_texture;
static const CAMetalLayer* g_swapchain;

namespace renderer
{

const char* sdlRenderDriverHint()
{
    return "metal";
}

void initialize( SDL_Window* window )
{
    g_renderer    = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    g_swapchain   = (__bridge CAMetalLayer *)SDL_RenderGetMetalLayer( g_renderer );
    g_device      = g_swapchain.device;
    g_queue       = [g_device newCommandQueue];

   // Build texture atlas
   ImGuiIO& io = ImGui::GetIO();
   unsigned char* pixels;
   int width, height;
   io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    texDesc.width = width;
    texDesc.height = height;

    g_texture = [g_device newTextureWithDescriptor:texDesc];
    MTLRegion region = { { 0, 0, 0 }, {(NSUInteger)width, (NSUInteger)height, 1} };
    [g_texture replaceRegion:region
             mipmapLevel:0
             withBytes:pixels
             bytesPerRow:4*width];

    io.Fonts->TexID = (void*)(intptr_t)g_texture;

    [texDesc release];
}

void terminate()
{
    SDL_DestroyRenderer( g_renderer );
}

void renderDrawlist( ImDrawData* draw_data )
{
    @autoreleasepool {
        MTLClearColor color = MTLClearColorMake(0, 0, 0, 1);
            id<CAMetalDrawable> surface = [g_swapchain nextDrawable];

            color.red = (color.red > 1.0) ? 0 : color.red + 0.01;

            MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].clearColor = color;
            pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].texture = surface.texture;

            id<MTLCommandBuffer> buffer = [g_queue commandBuffer];
            id<MTLRenderCommandEncoder> encoder = [buffer renderCommandEncoderWithDescriptor:pass];
            [encoder endEncoding];
            [buffer presentDrawable:surface];
            [buffer commit];
        }
    //id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
}

void setVSync( bool on )
{
   SDL_GL_SetSwapInterval( (int) on );
}

} // namespace renderer