// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Renderer.hpp"
#include "Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <fstream>
#include <vector>

namespace Nova {

    std::vector<uint8_t> Renderer::LoadSPIRV(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {};
        size_t size = file.tellg();
        if (size <= 0) return {};
        std::vector<uint8_t> buffer(size);
        file.seekg(0);
        file.read((char*)buffer.data(), size);
        return buffer;
    }

    void Renderer::InitPipelines() {
        auto vCode = LoadSPIRV("shaders/base.vert.spv");
        auto fCode = LoadSPIRV("shaders/base.frag.spv");

        SDL_GPUShaderCreateInfo vInfo = {
            .code_size = vCode.size(),
            .code = vCode.data(),
            .entrypoint = "main",
            .format = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 1, // Space 0, Slot 0
            .num_uniform_buffers = 1  // Lighting uniform slot 0
        };

        SDL_GPUShaderCreateInfo fInfo = vInfo;
        fInfo.code_size = fCode.size();
        fInfo.code = fCode.data();
        fInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fInfo.num_storage_buffers = 0;
        fInfo.num_uniform_buffers = 0;
        fInfo.num_samplers = 1;
        fInfo.num_storage_textures = 0;

        SDL_GPUShader* vShader = SDL_CreateGPUShader(device, &vInfo);
        SDL_GPUShader* fShader = SDL_CreateGPUShader(device, &fInfo);

        SDL_GPUGraphicsPipelineCreateInfo pInfo = {};
        pInfo.vertex_shader = vShader;
        pInfo.fragment_shader = fShader;
        pInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pInfo.depth_stencil_state.enable_depth_test = true;
        pInfo.depth_stencil_state.enable_depth_write = true;
        pInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;

        SDL_GPUVertexAttribute attrs[3];
        attrs[0] = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 }; // Position
        attrs[1] = { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, sizeof(glm::vec3) }; // Normal
        attrs[2] = { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, sizeof(glm::vec3) * 2 }; // UV

        SDL_GPUVertexBufferDescription desc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        pInfo.vertex_input_state.num_vertex_attributes = 3;
        pInfo.vertex_input_state.vertex_attributes = attrs;
        pInfo.vertex_input_state.num_vertex_buffers = 1;
        pInfo.vertex_input_state.vertex_buffer_descriptions = &desc;

        SDL_GPUColorTargetDescription colorDesc = { .format = SDL_GetGPUSwapchainTextureFormat(device, window) };
        colorDesc.blend_state.enable_blend = true;
        colorDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        colorDesc.blend_state.enable_color_write_mask = false;

        pInfo.target_info.num_color_targets = 1;
        pInfo.target_info.color_target_descriptions = &colorDesc;
        pInfo.target_info.has_depth_stencil_target = true;
        pInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;

        basePipeline = SDL_CreateGPUGraphicsPipeline(device, &pInfo);
        SDL_ReleaseGPUShader(device, vShader);
        SDL_ReleaseGPUShader(device, fShader);
        SDL_Log("Base pipeline initialized.");

        // Skybox Pipeline
        auto skyVCode = LoadSPIRV("shaders/skybox.vert.spv");
        auto skyFCode = LoadSPIRV("shaders/skybox.frag.spv");

        if (!skyVCode.empty() && !skyFCode.empty()) {
            SDL_Log("Skybox shaders loaded (%zu, %zu bytes).", skyVCode.size(), skyFCode.size());
            SDL_GPUShaderCreateInfo skyVInfo = {
                .code_size = skyVCode.size(),
                .code = skyVCode.data(),
                .entrypoint = "main",
                .format = SDL_GPU_SHADERFORMAT_SPIRV,
                .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                .num_uniform_buffers = 1 // MVP
            };
            SDL_GPUShaderCreateInfo skyFInfo = {
                .code_size = skyFCode.size(),
                .code = skyFCode.data(),
                .entrypoint = "main",
                .format = SDL_GPU_SHADERFORMAT_SPIRV,
                .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                .num_samplers = 1 // Cubemap
            };

            SDL_GPUShader* skyVShader = SDL_CreateGPUShader(device, &skyVInfo);
            SDL_GPUShader* skyFShader = SDL_CreateGPUShader(device, &skyFInfo);

            SDL_GPUGraphicsPipelineCreateInfo skyPInfo = pInfo;
            skyPInfo.vertex_shader = skyVShader;
            skyPInfo.fragment_shader = skyFShader;
            skyPInfo.depth_stencil_state.enable_depth_write = false;
            skyPInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
            skyPInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

            skyboxPipeline = SDL_CreateGPUGraphicsPipeline(device, &skyPInfo);
            if (skyboxPipeline) {
                SDL_Log("Skybox pipeline created successfully.");
            } else {
                SDL_Log("Failed to create skybox pipeline: %s", SDL_GetError());
            }

            SDL_ReleaseGPUShader(device, skyVShader);
            SDL_ReleaseGPUShader(device, skyFShader);
        } else {
            skyboxPipeline = nullptr;
            SDL_Log("Skybox shaders not found (expected shaders/skybox.vert.spv and shaders/skybox.frag.spv). Skybox will be disabled.");
        }
    }
}
