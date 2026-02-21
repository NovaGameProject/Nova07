// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Renderer.hpp"
#include "Geometry.hpp"
#include "Engine/Services/Lighting.hpp"
#include "Engine/Objects/Sky.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>

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
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            .width = tileW,
            .height = tileH,
            .layer_count_or_depth = 9,
            .num_levels = 7 // log2(64) + 1
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
        SDL_GenerateMipmapsForGPUTexture(cmd, surfaceTexture);
        auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, tBuf);
        SDL_ReleaseGPUTransferBuffer(device, ltBuf);
    }

    void Renderer::UpdateSkybox(std::shared_ptr<Workspace> workspace) {
        std::shared_ptr<Sky> sky = nullptr;
        
        // Find Sky in Lighting (standard location)
        if (auto parent = workspace->GetParent()) {
            for (auto& child : parent->GetChildren()) {
                if (auto lighting = std::dynamic_pointer_cast<Lighting>(child)) {
                    for (auto& lChild : lighting->GetChildren()) {
                        if (auto s = std::dynamic_pointer_cast<Sky>(lChild)) {
                            sky = s;
                            break;
                        }
                    }
                }
            }
        }

        if (!sky) {
            return;
        }

        bool changed = false;
        std::string newPaths[6] = {
            sky->props.SkyboxRt, sky->props.SkyboxLf,
            sky->props.SkyboxUp, sky->props.SkyboxDn,
            sky->props.SkyboxBk, sky->props.SkyboxFt
        };

        if (currentSkyboxPaths[0].empty()) changed = true; // Initial load

        for (int i = 0; i < 6; i++) {
            if (newPaths[i] != currentSkyboxPaths[i]) {
                changed = true;
                break;
            }
        }

        if (changed) {
            SDL_Log("Skybox changed or initializing...");
            std::vector<std::string> paths;
            for (int i = 0; i < 6; i++) {
                currentSkyboxPaths[i] = newPaths[i];
                std::string path = newPaths[i];
                if (path.starts_with("rbxasset://textures/sky/")) {
                    path = "resources/sky/" + path.substr(24);
                }
                paths.push_back(path);
                SDL_Log("  Face %d: %s", i, path.c_str());
            }
            LoadSkyboxTexture(paths);
        }
    }

    void Renderer::LoadSkyboxTexture(const std::vector<std::string>& paths) {
        if (paths.size() < 6) return;

        SDL_Surface* surfaces[6];
        bool success = true;
        for (int i = 0; i < 6; i++) {
            surfaces[i] = IMG_Load(paths[i].c_str());
            if (!surfaces[i]) {
                SDL_Log("Failed to load skybox texture %s: %s", paths[i].c_str(), SDL_GetError());
                success = false;
            } else {
                SDL_Surface* rgba = SDL_ConvertSurface(surfaces[i], SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(surfaces[i]);
                surfaces[i] = rgba;
            }
        }

        if (!success) {
            SDL_Log("Skybox loading failed due to missing textures.");
            for (int i = 0; i < 6; i++) if (surfaces[i]) SDL_DestroySurface(surfaces[i]);
            return;
        }

        if (skyboxTexture) SDL_ReleaseGPUTexture(device, skyboxTexture);

        uint32_t width = (uint32_t)surfaces[0]->w;
        uint32_t height = (uint32_t)surfaces[0]->h;
        uint32_t numLevels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;

        SDL_GPUTextureCreateInfo texInfo = {
            .type = SDL_GPU_TEXTURETYPE_CUBE,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            .width = width,
            .height = height,
            .layer_count_or_depth = 6,
            .num_levels = numLevels
        };
        skyboxTexture = SDL_CreateGPUTexture(device, &texInfo);

        uint32_t layerSize = width * height * 4;
        SDL_GPUTransferBufferCreateInfo ltInfo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = layerSize * 6 };
        auto ltBuf = SDL_CreateGPUTransferBuffer(device, &ltInfo);
        uint8_t* texData = (uint8_t*)SDL_MapGPUTransferBuffer(device, ltBuf, false);

        for (int i = 0; i < 6; i++) {
            std::memcpy(texData + (i * layerSize), surfaces[i]->pixels, layerSize);
            
            if (i == 2 || i == 3) {
                uint32_t* pixels = (uint32_t*)(texData + (i * layerSize));
                std::vector<uint32_t> temp(width * height);
                std::memcpy(temp.data(), pixels, layerSize);
                
                for (uint32_t y = 0; y < height; y++) {
                    for (uint32_t x = 0; x < width; x++) {
                        uint32_t srcIdx = y * width + x;
                        uint32_t dstIdx;
                        
                        if (i == 3) { // Bottom: 90 degrees clockwise
                            dstIdx = x * width + (width - 1 - y);
                        } else { // Top: 90 degrees counter-clockwise
                            dstIdx = (width - 1 - x) * width + y;
                        }
                        
                        pixels[dstIdx] = temp[srcIdx];
                    }
                }
            }
            
            SDL_DestroySurface(surfaces[i]);
        }
        SDL_UnmapGPUTransferBuffer(device, ltBuf);

        auto cmd = SDL_AcquireGPUCommandBuffer(device);
        auto copy = SDL_BeginGPUCopyPass(cmd);
        for (uint32_t i = 0; i < 6; i++) {
            SDL_GPUTextureTransferInfo texSrc = { ltBuf, layerSize * i };
            SDL_GPUTextureRegion texDst = { skyboxTexture, 0, i, 0, 0, 0, width, height, 1 };
            SDL_UploadToGPUTexture(copy, &texSrc, &texDst, false);
        }
        SDL_EndGPUCopyPass(copy);
        SDL_GenerateMipmapsForGPUTexture(cmd, skyboxTexture);
        auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, ltBuf);
        SDL_Log("Skybox cubemap created successfully (%dx%d).", width, height);
    }
}
