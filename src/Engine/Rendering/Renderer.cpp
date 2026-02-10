#include "Renderer.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/Camera.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <fstream>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace Nova {

    // Helper to find the active camera anywhere in the hierarchy
    static std::shared_ptr<Camera> FindCameraRecursive(std::shared_ptr<Instance> inst) {
        if (!inst) return nullptr;
        if (auto cam = std::dynamic_pointer_cast<Camera>(inst)) return cam;
        for (auto& child : inst->GetChildren()) {
            if (auto found = FindCameraRecursive(child)) return found;
        }
        return nullptr;
    }

    void Framebuffer::Refresh(SDL_GPUDevice* device, uint32_t w, uint32_t h) {
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

    void Framebuffer::Cleanup(SDL_GPUDevice* device) {
        if (depthTexture) SDL_ReleaseGPUTexture(device, depthTexture);
    }

    Renderer::Renderer(SDL_Window* window) : window(window) {
        device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
        SDL_ClaimWindowForGPUDevice(device, window);
        InitPipelines();
        CreateCubeResources();
    }

    Renderer::~Renderer() {
        SDL_ReleaseGPUGraphicsPipeline(device, basePipeline);
        SDL_ReleaseGPUBuffer(device, cubeBuffer);
        fb.Cleanup(device);
        SDL_DestroyGPUDevice(device);
    }

    void Renderer::RenderFrame(std::shared_ptr<Workspace> workspace) {
        auto cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUTexture* swapchainTexture;
        uint32_t w, h;

        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, &w, &h)) {
            fb.Refresh(device, w, h);

            // 1. Find the Camera
            std::shared_ptr<Camera> activeCamera = FindCameraRecursive(workspace);

            // 2. Math
            float fov = glm::radians(70.0f);
            float aspect = (float)w / (float)h;

            glm::mat4 proj = glm::perspective(fov, aspect, 0.1f, 1000.0f);

            glm::mat4 view = glm::mat4(1.0f);
            if (activeCamera) {
                view = activeCamera->GetViewMatrix();

                auto cf = activeCamera->props.CFrame.get().to_nova();
                static int frameCount = 0;
                if (frameCount++ % 60 == 0) {
                    SDL_Log("Active Camera: %s at (%.2f, %.2f, %.2f)",
                        activeCamera->GetName().c_str(), cf.position.x, cf.position.y, cf.position.z);
                }
            } else {
                view = glm::lookAt(glm::vec3(50, 50, 50), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            }

            glm::mat4 viewProj = proj * view;

            // 3. Render Pass (Explicit initialization matching working snippet)
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

                SubmitInstance(workspace, glm::mat4(1.0f), viewProj, cmd, pass);
            }
            SDL_EndGPURenderPass(pass);
            SDL_SubmitGPUCommandBuffer(cmd);
        } else {
            SDL_CancelGPUCommandBuffer(cmd);
        }
    }

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
            .num_storage_buffers = 0,
            .num_uniform_buffers = 2
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

        pInfo.depth_stencil_state.enable_depth_test = true;
        pInfo.depth_stencil_state.enable_depth_write = true;
        pInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;

        SDL_GPUVertexAttribute attr = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 };
        SDL_GPUVertexBufferDescription desc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        pInfo.vertex_input_state.num_vertex_attributes = 1;
        pInfo.vertex_input_state.vertex_attributes = &attr;
        pInfo.vertex_input_state.num_vertex_buffers = 1;
        pInfo.vertex_input_state.vertex_buffer_descriptions = &desc;

        SDL_GPUColorTargetDescription colorDesc = { .format = SDL_GetGPUSwapchainTextureFormat(device, window) };
        pInfo.target_info.num_color_targets = 1;
        pInfo.target_info.color_target_descriptions = &colorDesc;
        pInfo.target_info.has_depth_stencil_target = true;
        pInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

        pInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

        basePipeline = SDL_CreateGPUGraphicsPipeline(device, &pInfo);

        SDL_ReleaseGPUShader(device, vShader);
        SDL_ReleaseGPUShader(device, fShader);
    }

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

    void Renderer::SubmitInstance(
        std::shared_ptr<Instance> instance,
        const glm::mat4& parentTransform,
        const glm::mat4& viewProj,
        SDL_GPUCommandBuffer* cmd,
        SDL_GPURenderPass* pass
    ) {
        if (!instance) return;

        if (auto physical = std::dynamic_pointer_cast<BasePart>(instance)) {
            glm::mat4 worldMatrix = physical->GetLocalTransform();
            glm::mat4 scaledMatrix = glm::scale(worldMatrix, physical->GetSize());

            SceneUniforms ub = { viewProj * scaledMatrix };
            SDL_PushGPUVertexUniformData(cmd, 1, &ub, sizeof(SceneUniforms));
            SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
        }

        for (auto& child : instance->GetChildren()) {
            SubmitInstance(child, glm::mat4(1.0f), viewProj, cmd, pass);
        }
    }
}
