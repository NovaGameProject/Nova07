#include "Renderer.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/Camera.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/Lighting.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <cstring>
#include <algorithm>
#include <tracy/Tracy.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace Nova {

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

        uint32_t bufferSize = 16384 * sizeof(InstanceData);
        SDL_GPUBufferCreateInfo iInfo = {
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = bufferSize
        };
        instanceBuffer = SDL_CreateGPUBuffer(device, &iInfo);

        SDL_GPUTransferBufferCreateInfo tInfo = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = bufferSize
        };
        instanceTransferBuffer = SDL_CreateGPUTransferBuffer(device, &tInfo);

        SDL_GPUSamplerCreateInfo sInfo = {
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        };
        surfaceSampler = SDL_CreateGPUSampler(device, &sInfo);

        InitPipelines();
        CreateCubeResources();
    }

    Renderer::~Renderer() {
        SDL_ReleaseGPUGraphicsPipeline(device, basePipeline);
        SDL_ReleaseGPUBuffer(device, cubeBuffer);
        SDL_ReleaseGPUBuffer(device, instanceBuffer);
        if (instanceTransferBuffer) SDL_ReleaseGPUTransferBuffer(device, instanceTransferBuffer);
        if (surfaceSampler) SDL_ReleaseGPUSampler(device, surfaceSampler);
        if (surfaceTexture) SDL_ReleaseGPUTexture(device, surfaceTexture);
        fb.Cleanup(device);
        SDL_DestroyGPUDevice(device);
    }

    void Renderer::CollectInstances(
        std::shared_ptr<Workspace> workspace,
        const glm::mat4& viewProj,
        const Frustum& frustum,
        std::vector<InstanceData>& outData
    ) {
        glm::vec3 cameraPos(0, 0, 0);
        if (workspace->CurrentCamera) {
            cameraPos = workspace->CurrentCamera->props.CFrame.get().to_nova().position;
        }

        for (auto& physical : workspace->cachedParts) {
            glm::mat4 worldMatrix = physical->GetLocalTransform();
            glm::vec3 size = physical->GetSize();
            glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
            float radius = glm::length(size) * 0.5f;

            if (frustum.IntersectsSphere(worldPos, radius)) {
                glm::mat4 scaledMatrix = glm::scale(worldMatrix, size);
                auto& bp = physical->basePartProps;
                InstanceData data = {
                    viewProj * scaledMatrix,
                    scaledMatrix,
                    physical->GetColor(),
                    glm::vec4(size, 1.0f),
                    {
                        (int32_t)bp->FrontSurface, (int32_t)bp->BackSurface,
                        (int32_t)bp->LeftSurface, (int32_t)bp->RightSurface,
                        (int32_t)bp->TopSurface, (int32_t)bp->BottomSurface,
                        0, 0 // Padding
                    }
                };
                outData.push_back(data);
            }
        }
    }

    void Renderer::RenderFrame(std::shared_ptr<Workspace> workspace) {
        auto cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUTexture* swapchainTexture;
        uint32_t w, h;

        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, &w, &h)) {
            fb.Refresh(device, w, h);

            float aspect = (float)w / (float)h;
            glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(70.0f), aspect, 0.1f, 10000.0f);

            glm::mat4 view = glm::mat4(1.0f);
            glm::vec3 cameraPos(50, 50, 50);
            if (workspace->CurrentCamera) {
                view = workspace->CurrentCamera->GetViewMatrix();
                cameraPos = workspace->CurrentCamera->props.CFrame.get().to_nova().position;
            } else {
                view = glm::lookAtRH(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            }

            glm::mat4 viewProj = proj * view;
            Frustum frustum;
            frustum.Extract(viewProj);

            std::shared_ptr<Lighting> lighting = nullptr;
            if (auto parent = workspace->GetParent()) {
                for (auto& child : parent->GetChildren()) {
                    if (auto l = std::dynamic_pointer_cast<Lighting>(child)) {
                        lighting = l;
                        break;
                    }
                }
            }

            LightingData lData;
            if (lighting) {
                lData.topAmbient = glm::vec4(lighting->props.TopAmbientV9.to_glm(), 1.0f);
                lData.bottomAmbient = glm::vec4(lighting->props.BottomAmbientV9.to_glm(), 1.0f);
                lData.lightDir = glm::vec4(glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)), 1.0f);
            } else {
                lData.topAmbient = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                lData.bottomAmbient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
                lData.lightDir = glm::vec4(glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)), 1.0f);
            }

            std::vector<InstanceData> instances;
            instances.reserve(workspace->cachedParts.size());
            CollectInstances(workspace, viewProj, frustum, instances);

            // Optimization: Partition opaque and transparent
            auto transparentIt = std::partition(instances.begin(), instances.end(), [](const InstanceData& d) {
                return d.color.a >= 0.99f; // Opaque first
            });

            // Only sort the transparent range (back-to-front)
            std::sort(transparentIt, instances.end(), [&](const InstanceData& a, const InstanceData& b) {
                float distA = glm::distance2(glm::vec3(a.model[3]), cameraPos);
                float distB = glm::distance2(glm::vec3(b.model[3]), cameraPos);
                return distA > distB;
            });

            auto copyCmd = SDL_BeginGPUCopyPass(cmd);
            if (!instances.empty()) {
                uint32_t dataSize = instances.size() * sizeof(InstanceData);
                uint32_t maxBufferSize = 16384 * sizeof(InstanceData);
                if (dataSize > maxBufferSize) dataSize = maxBufferSize;

                void* data = SDL_MapGPUTransferBuffer(device, instanceTransferBuffer, false);
                std::memcpy(data, instances.data(), dataSize);
                SDL_UnmapGPUTransferBuffer(device, instanceTransferBuffer);

                SDL_GPUTransferBufferLocation src = { instanceTransferBuffer, 0 };
                SDL_GPUBufferRegion dst = { instanceBuffer, 0, dataSize };
                SDL_UploadToGPUBuffer(copyCmd, &src, &dst, true);
            }
            SDL_EndGPUCopyPass(copyCmd);

            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = swapchainTexture;
            colorTarget.clear_color = { 0.1f, 0.1f, 0.2f, 1.0f };
            if (lighting) {
                auto& cc = lighting->props.ClearColor;
                colorTarget.clear_color = { cc.r, cc.g, cc.b, 1.0f };
            }
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo depthTarget = {};
            depthTarget.texture = fb.depthTexture;
            depthTarget.clear_depth = 1.0f;
            depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;

            auto pass = SDL_BeginGPURenderPass(cmd, &colorTarget, 1, &depthTarget);
            if (!instances.empty()) {
                SDL_BindGPUGraphicsPipeline(pass, basePipeline);
                SDL_GPUBufferBinding vBinding = { .buffer = cubeBuffer, .offset = 0 };
                SDL_BindGPUVertexBuffers(pass, 0, &vBinding, 1);
                SDL_BindGPUVertexStorageBuffers(pass, 0, &instanceBuffer, 1);

                // Bind Surface Textures to Fragment Stage (Set 2, Slot 0)
                SDL_GPUTextureSamplerBinding tBinding = { .texture = surfaceTexture, .sampler = surfaceSampler };
                SDL_BindGPUFragmentSamplers(pass, 0, &tBinding, 1);

                SDL_PushGPUVertexUniformData(cmd, 0, &lData, sizeof(LightingData));
                SDL_DrawGPUPrimitives(pass, 36, instances.size(), 0, 0);
            }
            SDL_EndGPURenderPass(pass);
            SDL_SubmitGPUCommandBuffer(cmd);
        } else {
            SDL_CancelGPUCommandBuffer(cmd);
        }
    }
}
