#include "Renderer.hpp"
#include "Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Nova {

    void Renderer::CreateCubeResources() {
        Mesh mesh = Geometry::CreateCube();
        Uint32 size = mesh.vertices.size() * sizeof(Vertex);
        SDL_GPUBufferCreateInfo bInfo = { .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size };
        cubeBuffer = SDL_CreateGPUBuffer(device, &bInfo);
        SDL_GPUTransferBufferCreateInfo tInfo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size };
        auto tBuf = SDL_CreateGPUTransferBuffer(device, &tInfo);
        void* data = SDL_MapGPUTransferBuffer(device, tBuf, false);
        std::memcpy(data, mesh.vertices.data(), size);
        SDL_UnmapGPUTransferBuffer(device, tBuf);

        // Load Surfaces.png using SDL_image (64x512)
        SDL_Surface* atlas = IMG_Load("resources/textures/Surfaces.png");
        if (!atlas) {
            SDL_Log("Failed to load Surfaces.png: %s", SDL_GetError());
            // Create a default white surface if file is missing or unsupported
            atlas = SDL_CreateSurface(64, 64 * 8, SDL_PIXELFORMAT_RGBA32);
            SDL_FillSurfaceRect(atlas, NULL, SDL_MapSurfaceRGBA(atlas, 255, 255, 255, 255));
        }

        SDL_Log("Loaded Surfaces.png: %dx%d", atlas->w, atlas->h);

        SDL_Surface* atlasRGBA = SDL_ConvertSurface(atlas, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(atlas);

        if (!atlasRGBA) {
            SDL_Log("Failed to convert surface to RGBA32: %s", SDL_GetError());
            return;
        }

        uint32_t tileW = 64;
        uint32_t tileH = 64; 
        uint32_t atlasSlices = atlasRGBA->h / tileH;

        // Surface Texture Array (9 layers: 0=Smooth, 1-8=Atlas Tiles)
        SDL_GPUTextureCreateInfo texInfo = {
            .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = tileW,
            .height = tileH,
            .layer_count_or_depth = 9,
            .num_levels = 1
        };
        surfaceTexture = SDL_CreateGPUTexture(device, &texInfo);

        uint32_t layerSize = tileW * tileH * 4;
        SDL_GPUTransferBufferCreateInfo ltInfo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = layerSize * 9 };
        auto ltBuf = SDL_CreateGPUTransferBuffer(device, &ltInfo);
        uint8_t* texData = (uint8_t*)SDL_MapGPUTransferBuffer(device, ltBuf, false);
        
        // 1. Fill all layers with white initially (Smooth)
        std::memset(texData, 255, layerSize * 9); 

        // 2. Copy the actual tiles from the atlas using explicit mapping
        // Layout: 0,1:Studs, 2,3:Inlets, 4,5:Weld, 6,7:Glue
        auto copyTile = [&](int srcIdx, int dstIdx) {
            if (srcIdx < 0 || srcIdx >= (int)atlasSlices) return;
            uint8_t* srcPtr = (uint8_t*)atlasRGBA->pixels + (srcIdx * layerSize);
            uint8_t* dstPtr = texData + (dstIdx * layerSize);
            std::memcpy(dstPtr, srcPtr, layerSize);
        };

        copyTile(0, 3); // Studs (Enum 3)
        copyTile(2, 4); // Inlets (Enum 4)
        copyTile(4, 2); // Weld (Enum 2)
        copyTile(6, 1); // Glue (Enum 1)

        SDL_UnmapGPUTransferBuffer(device, ltBuf);
        SDL_DestroySurface(atlasRGBA);

        auto cmd = SDL_AcquireGPUCommandBuffer(device);
        auto copy = SDL_BeginGPUCopyPass(cmd);
        
        SDL_GPUTransferBufferLocation gsrc = { tBuf, 0 };
        SDL_GPUBufferRegion gdst = { cubeBuffer, 0, size };
        SDL_UploadToGPUBuffer(copy, &gsrc, &gdst, false);

        for (uint32_t i = 0; i < 9; i++) {
            SDL_GPUTextureTransferInfo texSrc = { ltBuf, layerSize * i };
            SDL_GPUTextureRegion texDst = { surfaceTexture, 0, i, 0, 0, 0, tileW, tileH, 1 };
            SDL_UploadToGPUTexture(copy, &texSrc, &texDst, false);
        }

        SDL_EndGPUCopyPass(copy);
        auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, tBuf);
        SDL_ReleaseGPUTransferBuffer(device, ltBuf);
    }
}
