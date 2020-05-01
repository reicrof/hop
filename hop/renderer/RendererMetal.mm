#include "hop/Renderer.h"
#include "common/Utils.h"

#include "imgui/imgui.h"

#include <cmath>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL.h>

static SDL_Renderer* g_renderer;
static id<MTLDevice> g_device;
static id<MTLCommandQueue> g_queue;
static id<MTLTexture> g_texture;
static id<MTLRenderPipelineState> g_pipelineState;
static CAMetalLayer* g_swapchain;

static constexpr uint32_t MAX_FRAME_IN_FLIGHT     = 3;
static constexpr uint32_t DEFAULT_GPU_BUFFER_SIZE = 16;//1024 * 1024U;
struct GPUBuffers
{
   id<MTLBuffer> vertex;
   id<MTLBuffer> index;
};
static GPUBuffers g_buffersPerFrame[3];

static NSString* shaderSrc =
    @""
     "#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "\n"
     "struct Uniforms {\n"
     "    float4x4 projectionMatrix;\n"
     "};\n"
     "\n"
     "struct VertexIn {\n"
     "    float2 position  [[attribute(0)]];\n"
     "    float2 texCoords [[attribute(1)]];\n"
     "    uchar4 color     [[attribute(2)]];\n"
     "};\n"
     "\n"
     "struct VertexOut {\n"
     "    float4 position [[position]];\n"
     "    float2 texCoords;\n"
     "    float4 color;\n"
     "};\n"
     "\n"
     "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
     "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
     "    VertexOut out;\n"
     "    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);\n"
     "    out.texCoords = in.texCoords;\n"
     "    out.color = float4(in.color) / float4(255.0);\n"
     "    return out;\n"
     "}\n"
     "\n"
     "fragment half4 fragment_main(VertexOut in [[stage_in]],\n"
     "                             texture2d<half, access::sample> texture [[texture(0)]]) {\n"
     "    constexpr sampler linearSampler(coord::normalized, min_filter::linear, "
     "mag_filter::linear, mip_filter::linear);\n"
     "    half4 texColor = texture.sample(linearSampler, in.texCoords);\n"
     "    return half4(in.color) * texColor;\n"
     "}\n";

static MTLPixelFormat sdlPxlFmtToMtlPxlFmt( uint32_t fmt )
{
   // clang-format off
   switch( fmt )
   {
      case SDL_PIXELFORMAT_ARGB8888: // Fallthrough
      case SDL_PIXELFORMAT_RGBA8888: return MTLPixelFormatRGBA8Unorm;
      case SDL_PIXELFORMAT_BGRA8888: return MTLPixelFormatBGRA8Unorm;
      case SDL_PIXELFORMAT_UNKNOWN:  return MTLPixelFormatInvalid;
   }
   // clang-format on
   return MTLPixelFormatInvalid;
}

static void setOrthoMatrix(
    float left,
    float right,
    float bottom,
    float top,
    float zNear,
    float zFar,
    float mat[4][4] )
{
   memset( mat, 0, 16 * sizeof(float) );
   mat[0][0] = 2.0f / ( right - left );
   mat[1][1] = 2.0f / ( top - bottom );
   mat[2][2] = 1.0f / ( zFar - zNear );
   mat[3][0] = (left + right) / (left - right);
   mat[3][1] = (top + bottom) / (bottom - top);
   mat[3][2] = zNear / (zNear - zFar);
   mat[3][3] = 1.0f;
}

// Acquire buffers from the current frame and reallocate them if they are too small.
static GPUBuffers&
acquireBuffersForFrame( uint32_t frameIdx, uint32_t vertexSize, uint32_t indexSize )
{
   GPUBuffers& curBuf         = g_buffersPerFrame[frameIdx];
   const uint32_t curVertSize = [curBuf.vertex length];
   const uint32_t curIdxSize  = [curBuf.index length];
   if( vertexSize > curVertSize )
   {
      const uint32_t nextSz = vertexSize;// hop::nextPow2( vertexSize );
      curBuf.vertex = [g_device newBufferWithLength:nextSz options:MTLResourceStorageModeManaged];
   }
   if( indexSize > curIdxSize )
   {
      const uint32_t nextSz = indexSize;//hop::nextPow2( indexSize );
      curBuf.index = [g_device newBufferWithLength:nextSz options:MTLResourceStorageModeManaged];
   }

   return curBuf;
}

namespace renderer
{
const char* sdlRenderDriverHint() { return "metal"; }

bool initialize( SDL_Window* window )
{
   g_renderer  = SDL_CreateRenderer( window, -1, SDL_RENDERER_PRESENTVSYNC );
   g_swapchain = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer( g_renderer );
   g_device    = g_swapchain.device;
   g_queue     = [g_device newCommandQueue];

   {
      NSError* error = nil;

      id<MTLLibrary> library = [g_device newLibraryWithSource:shaderSrc options:nil error:&error];
      if( library == nil )
      {
         NSLog( @"Error: failed to create Metal library: %@", error );
         return false;
      }

      id<MTLFunction> vertexFunction   = [library newFunctionWithName:@"vertex_main"];
      id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];
      if( vertexFunction == nil || fragmentFunction == nil )
      {
         NSLog( @"Error: failed to find Metal shader functions in library: %@", error );
         return false;
      }

      MTLVertexDescriptor* vertexDesc      = [MTLVertexDescriptor vertexDescriptor];
      vertexDesc.attributes[0].offset      = IM_OFFSETOF( ImDrawVert, pos );
      vertexDesc.attributes[0].format      = MTLVertexFormatFloat2;  // position
      vertexDesc.attributes[0].bufferIndex = 0;
      vertexDesc.attributes[1].offset      = IM_OFFSETOF( ImDrawVert, uv );
      vertexDesc.attributes[1].format      = MTLVertexFormatFloat2;  // texCoords
      vertexDesc.attributes[1].bufferIndex = 0;
      vertexDesc.attributes[2].offset      = IM_OFFSETOF( ImDrawVert, col );
      vertexDesc.attributes[2].format      = MTLVertexFormatUChar4;  // color
      vertexDesc.attributes[2].bufferIndex = 0;
      vertexDesc.layouts[0].stepRate       = 1;
      vertexDesc.layouts[0].stepFunction   = MTLVertexStepFunctionPerVertex;
      vertexDesc.layouts[0].stride         = sizeof( ImDrawVert );

      const uint32_t sdlPxlFmt    = SDL_GetWindowPixelFormat( window );
      const MTLPixelFormat mtlFmt = sdlPxlFmtToMtlPxlFmt( sdlPxlFmt );
      if( mtlFmt == MTLPixelFormatInvalid )
      {
         NSLog( @"Surface Pixel Format %s not supported\n", SDL_GetPixelFormatName( sdlPxlFmt ) );
         return false;
      }
      assert( mtlFmt != MTLPixelFormatInvalid );

      MTLRenderPipelineDescriptor* pipelineDesc        = [[MTLRenderPipelineDescriptor alloc] init];
      pipelineDesc.vertexFunction                      = vertexFunction;
      pipelineDesc.fragmentFunction                    = fragmentFunction;
      pipelineDesc.vertexDescriptor                    = vertexDesc;
      pipelineDesc.sampleCount                         = 1;
      pipelineDesc.colorAttachments[0].pixelFormat     = MTLPixelFormatBGRA8Unorm;
      pipelineDesc.colorAttachments[0].blendingEnabled = YES;
      pipelineDesc.colorAttachments[0].rgbBlendOperation      = MTLBlendOperationAdd;
      pipelineDesc.colorAttachments[0].alphaBlendOperation    = MTLBlendOperationAdd;
      pipelineDesc.colorAttachments[0].sourceRGBBlendFactor   = MTLBlendFactorSourceAlpha;
      pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
      pipelineDesc.colorAttachments[0].destinationRGBBlendFactor =
          MTLBlendFactorOneMinusSourceAlpha;
      pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor =
          MTLBlendFactorOneMinusSourceAlpha;

      g_pipelineState = [g_device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
      if( error != nil )
      {
         NSLog( @"Error: failed to create Metal pipeline state: %@", error );
         return false;
      }
   }

   // Allocate GPU buffers for each frame
   for( uint32_t i = 0; i < MAX_FRAME_IN_FLIGHT; ++i )
   {
      g_buffersPerFrame[i].vertex = [g_device newBufferWithLength:DEFAULT_GPU_BUFFER_SIZE
                                                          options:MTLResourceStorageModeManaged];
      g_buffersPerFrame[i].index  = [g_device newBufferWithLength:DEFAULT_GPU_BUFFER_SIZE
                                                         options:MTLResourceStorageModeManaged];
   }

   // Build texture atlas
   {
      ImGuiIO& io = ImGui::GetIO();
      unsigned char* pixels;
      int width, height;
      io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

      MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
      texDesc.pixelFormat           = MTLPixelFormatRGBA8Unorm;
      texDesc.width                 = width;
      texDesc.height                = height;
      texDesc.usage                 = MTLTextureUsageShaderRead;
      texDesc.storageMode           = MTLStorageModeManaged;

      g_texture = [g_device newTextureWithDescriptor:texDesc];
      [texDesc release];

      MTLRegion region = {{0, 0, 0}, {(NSUInteger)width, (NSUInteger)height, 1}};
      [g_texture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:4 * width];
      io.Fonts->TexID = (void*)(intptr_t)g_texture;
   }

   return true;
}

void terminate() { SDL_DestroyRenderer( g_renderer ); }

void renderDrawlist( ImDrawData* drawData )
{
   static uint32_t curFrame = 0;
   @autoreleasepool
   {
      const size_t vertBufferSize = drawData->TotalVtxCount * sizeof( ImDrawVert );
      const size_t idxBufferSize  = drawData->TotalIdxCount * sizeof( ImDrawIdx );

      GPUBuffers& gpuBuffers = acquireBuffersForFrame( curFrame, vertBufferSize, idxBufferSize );

      {  // Copy vertices/indices to gpu buffers
         ImDrawVert* vertexData  = (ImDrawVert*)[gpuBuffers.vertex contents];
         ImDrawIdx* indexData    = (ImDrawIdx*)[gpuBuffers.index contents];
         uint32_t curVertexCount = 0;
         uint32_t curIndexCount  = 0;
         for( int n = 0; n < drawData->CmdListsCount; n++ )
         {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(
                vertexData + curVertexCount,
                cmdList->VtxBuffer.Data,
                cmdList->VtxBuffer.Size * sizeof( ImDrawVert ) );
            memcpy(
                indexData + curIndexCount,
                cmdList->IdxBuffer.Data,
                cmdList->IdxBuffer.Size * sizeof( ImDrawIdx ) );
            curVertexCount += cmdList->VtxBuffer.Size;
            curIndexCount += cmdList->IdxBuffer.Size;
         }
         [gpuBuffers.vertex didModifyRange:NSMakeRange( 0, vertBufferSize )];
         [gpuBuffers.index didModifyRange:NSMakeRange( 0, idxBufferSize )];
      }

      id<CAMetalDrawable> surface = [g_swapchain nextDrawable];
      const float fbWidth  = [surface.texture width];
      const float fbHeight = [surface.texture height];

      MTLViewport viewport = {};
      viewport.width       = fbWidth;
      viewport.height      = fbHeight;
      viewport.zfar        = 1.0;

      MTLScissorRect sc = {};
      sc.width  = (int)fbWidth;
      sc.height = (int)fbHeight;

      MTLRenderPassDescriptor* pass        = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].clearColor  = MTLClearColorMake( 0, 0, 0, 1 );
      pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].texture = surface.texture;

      id<MTLCommandBuffer> buffer         = [g_queue commandBuffer];
      id<MTLRenderCommandEncoder> encoder = [buffer renderCommandEncoderWithDescriptor:pass];

      [encoder setRenderPipelineState:g_pipelineState];
      [encoder setViewport:viewport];
      [encoder setScissorRect:sc];
      [encoder setVertexBuffer:gpuBuffers.vertex offset:0 atIndex:0];

      float orthoMat[4][4];
      setOrthoMatrix( 0.0f, fbWidth, fbHeight, 0.0f, -1.0f, 1.0f, orthoMat );
      [encoder setVertexBytes:&orthoMat length:sizeof( orthoMat ) atIndex:1];

      constexpr MTLIndexType idxType =
          sizeof( ImDrawIdx ) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
      uint32_t vertexOffset = 0;
      uint32_t indexOffset  = 0;
      for( int n = 0; n < drawData->CmdListsCount; n++ )
      {
         const ImDrawList* cmdList = drawData->CmdLists[n];

         [encoder setVertexBufferOffset:( vertexOffset * sizeof( ImDrawVert ) ) atIndex:0];
         for( int i = 0; i < cmdList->CmdBuffer.Size; i++ )
         {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[i];
            assert( !cmd->UserCallback );  // We do not support user callback yet.

            [encoder setFragmentTexture:(id<MTLTexture>)(cmd->TextureId)atIndex:0];
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:cmd->ElemCount
                                 indexType:idxType
                               indexBuffer:gpuBuffers.index
                         indexBufferOffset:indexOffset * sizeof( ImDrawIdx )];
            indexOffset += cmd->ElemCount;
         }
         vertexOffset += cmdList->VtxBuffer.Size;
      }

      [encoder endEncoding];
      [buffer presentDrawable:surface];
      [buffer commit];
   }

   curFrame = ( curFrame + 1 ) % MAX_FRAME_IN_FLIGHT;
}

void setVSync( bool on )
{
   g_swapchain.displaySyncEnabled = on;
}

}  // namespace renderer
