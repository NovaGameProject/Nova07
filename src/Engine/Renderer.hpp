#pragma once
#include "Engine/Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <string>
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Nova {
    struct SceneUniforms {
        glm::mat4 mvp;
    };

    // Manages window-dependent resources like Depth Buffers
    struct Framebuffer {
        SDL_GPUTexture* depthTexture = nullptr;
        Uint32 width = 0, height = 0;

        void Refresh(SDL_GPUDevice* device, Uint32 w, Uint32 h) {
            if (w == width && h == height && depthTexture != nullptr) return;

            if (depthTexture) SDL_ReleaseGPUTexture(device, depthTexture);

            SDL_GPUTextureCreateInfo info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
                .width = w,
                .height = h,
                .layer_count_or_depth = 1,
                .num_levels = 1
            };
            depthTexture = SDL_CreateGPUTexture(device, &info);
            width = w;
            height = h;
            SDL_Log("Depth buffer recreated: %dx%d", w, h);
        }

        void Cleanup(SDL_GPUDevice* device) {
            if (depthTexture) SDL_ReleaseGPUTexture(device, depthTexture);
        }
    };

    class Renderer {
    public:
        Renderer(SDL_Window* window) : window(window) {
            device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
            SDL_ClaimWindowForGPUDevice(device, window);

            InitPipelines();
            CreateCubeResources();
        }

        ~Renderer() {
            SDL_ReleaseGPUGraphicsPipeline(device, basePipeline);
            SDL_ReleaseGPUBuffer(device, cubeBuffer);
            fb.Cleanup(device);
            SDL_DestroyGPUDevice(device);
        }

        void RenderFrame() {
            auto cmd = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUTexture* swapchainTexture;
            Uint32 w, h;

            if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, &w, &h)) {
                // 1. Manage Resources
                fb.Refresh(device, w, h);

                // 2. Math (The "Engine" Logic)
                float aspect = (float)w / (float)h;
                glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
                // proj[1][1] *= -1; // Uncomment if your cube looks upside down again

                glm::mat4 view = glm::lookAt(glm::vec3(2, 2, 4), glm::vec3(0), glm::vec3(0, 1, 0));

                static float rot = 0.0f;
                rot += 0.01f;
                glm::mat4 model = glm::rotate(glm::mat4(1.0f), rot, glm::vec3(0, 1, 0));

                SceneUniforms ub = { proj * view * model };
                SDL_PushGPUVertexUniformData(cmd, 0, &ub, sizeof(SceneUniforms));

                // 3. Render Pass
                SDL_GPUColorTargetInfo colorTarget = {};
                colorTarget.texture = swapchainTexture;
                colorTarget.clear_color = { 0.1f, 0.1f, 0.2f, 1.0f };
                colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
                colorTarget.store_op = SDL_GPU_STOREOP_STORE;

                SDL_GPUDepthStencilTargetInfo depthTarget = {};
                depthTarget.texture = fb.depthTexture;
                depthTarget.clear_depth = 1.0f;
                depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
                depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;

                auto pass = SDL_BeginGPURenderPass(cmd, &colorTarget, 1, &depthTarget);
                {
                    SDL_BindGPUGraphicsPipeline(pass, basePipeline);
                    SDL_GPUBufferBinding binding = { .buffer = cubeBuffer, .offset = 0 };
                    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
                    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
                }
                SDL_EndGPURenderPass(pass);
                SDL_SubmitGPUCommandBuffer(cmd);
            } else {
                SDL_CancelGPUCommandBuffer(cmd);
            }
        }

    private:
        SDL_GPUDevice* device;
        SDL_Window* window;
        SDL_GPUGraphicsPipeline* basePipeline;
        SDL_GPUBuffer* cubeBuffer;
        Framebuffer fb;

        // Helper to load SPIR-V files
        std::vector<Uint8> LoadSPIRV(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);

            if (!file.is_open()) {
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "CRITICAL: Could not find shader file: %s", path.c_str());
                // Return an empty vector so we don't try to allocate 18 quintillion bytes
                return {};
            }

            std::streamsize size = file.tellg();
            if (size <= 0) {
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "CRITICAL: Shader file is empty: %s", path.c_str());
                return {};
            }

            std::vector<Uint8> buffer(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(buffer.data()), size);
            file.close();

            return buffer;
        }

        void InitPipelines() {
            auto vCode = LoadSPIRV("shaders/base.vert.spv");
            auto fCode = LoadSPIRV("shaders/base.frag.spv");

            // Correct order based on the struct you provided
            SDL_GPUShaderCreateInfo vInfo = {
                .code_size = vCode.size(),
                .code = vCode.data(),
                .entrypoint = "main",
                .format = SDL_GPU_SHADERFORMAT_SPIRV,
                .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                .num_samplers = 0,
                .num_storage_textures = 0,
                .num_storage_buffers = 0,
                .num_uniform_buffers = 2 // Authorizes space1 for Slot 0
            };

            SDL_GPUShaderCreateInfo fInfo = vInfo;
            fInfo.code_size = fCode.size();
            fInfo.code = fCode.data();
            fInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            fInfo.num_uniform_buffers = 0;

            SDL_GPUShader* vShader = SDL_CreateGPUShader(device, &vInfo);
            SDL_GPUShader* fShader = SDL_CreateGPUShader(device, &fInfo);

            SDL_GPUGraphicsPipelineCreateInfo pInfo = {};
            pInfo.vertex_shader = vShader;
            pInfo.fragment_shader = fShader;
            pInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

            // Depth State
            pInfo.depth_stencil_state.enable_depth_test = true;
            pInfo.depth_stencil_state.enable_depth_write = true;
            pInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;

            // Vertex Input
            SDL_GPUVertexAttribute attr = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 };
            SDL_GPUVertexBufferDescription desc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
            pInfo.vertex_input_state.num_vertex_attributes = 1;
            pInfo.vertex_input_state.vertex_attributes = &attr;
            pInfo.vertex_input_state.num_vertex_buffers = 1;
            pInfo.vertex_input_state.vertex_buffer_descriptions = &desc;

            // Targets
            SDL_GPUColorTargetDescription colorDesc = { .format = SDL_GetGPUSwapchainTextureFormat(device, window) };
            pInfo.target_info.num_color_targets = 1;
            pInfo.target_info.color_target_descriptions = &colorDesc;
            pInfo.target_info.has_depth_stencil_target = true;
            pInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

            basePipeline = SDL_CreateGPUGraphicsPipeline(device, &pInfo);

            SDL_ReleaseGPUShader(device, vShader);
            SDL_ReleaseGPUShader(device, fShader);
        }

        void CreateCubeResources() {
            Mesh mesh = Geometry::CreateCube();
            Uint32 size = mesh.vertices.size() * sizeof(Vertex);

            SDL_GPUBufferCreateInfo bInfo = { .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size };
            cubeBuffer = SDL_CreateGPUBuffer(device, &bInfo);

            SDL_GPUTransferBufferCreateInfo tInfo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size };
            auto tBuf = SDL_CreateGPUTransferBuffer(device, &tInfo);

            void* data = SDL_MapGPUTransferBuffer(device, tBuf, false);
            std::memcpy(data, mesh.vertices.data(), size);
            SDL_UnmapGPUTransferBuffer(device, tBuf);

            auto cmd = SDL_AcquireGPUCommandBuffer(device);
            auto copy = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTransferBufferLocation src = { tBuf, 0 };
            SDL_GPUBufferRegion dst = { cubeBuffer, 0, size };
            SDL_UploadToGPUBuffer(copy, &src, &dst, false);
            SDL_EndGPUCopyPass(copy);

            auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
            SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
            SDL_ReleaseGPUTransferBuffer(device, tBuf);
        }
    };
}
