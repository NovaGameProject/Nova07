// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Frustum.hpp"
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Nova {
    struct Vertex;

    struct Framebuffer {
        SDL_GPUTexture* depthTexture = nullptr;
        uint32_t width = 0, height = 0;

        void Refresh(SDL_GPUDevice* device, uint32_t w, uint32_t h);
        void Cleanup(SDL_GPUDevice* device);
    };

    struct LightingData {
        glm::vec4 topAmbient;
        glm::vec4 bottomAmbient;
        glm::vec4 lightDir;
        glm::vec4 fogColor;
        glm::vec4 fogParams; // x = start, y = end, z = enabled
        glm::vec4 cameraPos;
    };

    class Renderer {
    public:
        Renderer(SDL_Window* window);
        ~Renderer();

        struct InstanceData {
            glm::mat4 mvp;
            glm::mat4 model;
            glm::vec4 color;
            glm::vec4 scale;
            int32_t surfaces[8];
        };

        void RenderFrame(std::shared_ptr<Workspace> root);

    private:
        void CollectInstances(std::shared_ptr<Workspace> workspace,
            const glm::mat4& viewProj,
            const Frustum& frustum,
            std::vector<InstanceData>& outData);

        void UpdateSkybox(std::shared_ptr<Workspace> workspace);
        void LoadSkyboxTexture(const std::vector<std::string>& paths);

        SDL_GPUDevice* device;
        SDL_Window* window;
        SDL_GPUGraphicsPipeline* basePipeline;
        SDL_GPUGraphicsPipeline* skyboxPipeline;
        SDL_GPUBuffer* cubeBuffer;
        SDL_GPUBuffer* instanceBuffer;
        SDL_GPUTransferBuffer* instanceTransferBuffer = nullptr;

        SDL_GPUTexture* surfaceTexture = nullptr;
        SDL_GPUSampler* surfaceSampler = nullptr;

        SDL_GPUTexture* skyboxTexture = nullptr;
        std::string currentSkyboxPaths[6];

        Framebuffer fb;

        std::vector<uint8_t> LoadSPIRV(const std::string& path);
        void InitPipelines();
        void CreateCubeResources();
    };
}
