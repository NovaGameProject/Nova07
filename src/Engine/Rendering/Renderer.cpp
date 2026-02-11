#include "Renderer.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/Camera.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/Lighting.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Geometry.hpp"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <fstream>
#include <cstring>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace Nova {

    struct Frustum {
        glm::vec4 planes[6];

        void Extract(const glm::mat4& m) {
            planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // Left
            planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // Right
            planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // Bottom
            planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // Top
            planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]); // Near
            planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // Far

            for (int i = 0; i < 6; i++) {
                float length = glm::length(glm::vec3(planes[i]));
                planes[i] /= length;
            }
        }

        bool IntersectsSphere(const glm::vec3& center, float radius) const {
            for (int i = 0; i < 6; i++) {
                if (glm::dot(glm::vec3(planes[i]), center) + planes[i].w < -radius) {
                    return false;
                }
            }
            return true;
        }
    };

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

        InitPipelines();
        CreateCubeResources();
    }

    Renderer::~Renderer() {
        SDL_ReleaseGPUGraphicsPipeline(device, basePipeline);
        SDL_ReleaseGPUBuffer(device, cubeBuffer);
        SDL_ReleaseGPUBuffer(device, instanceBuffer);
        if (instanceTransferBuffer) SDL_ReleaseGPUTransferBuffer(device, instanceTransferBuffer);
        fb.Cleanup(device);
        SDL_DestroyGPUDevice(device);
    }

    void Renderer::CollectInstances(
        std::shared_ptr<Instance> instance,
        const glm::mat4& viewProj,
        const Frustum& frustum,
        std::vector<InstanceData>& outData
    ) {
        if (!instance) return;

        if (auto physical = std::dynamic_pointer_cast<BasePart>(instance)) {
            glm::mat4 worldMatrix = physical->GetLocalTransform();
            glm::vec3 size = physical->GetSize();
            glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
            float radius = glm::length(size) * 0.5f;

            if (frustum.IntersectsSphere(worldPos, radius)) {
                glm::mat4 scaledMatrix = glm::scale(worldMatrix, size);
                outData.push_back({
                    viewProj * scaledMatrix,
                    scaledMatrix,
                    physical->GetColor()
                });
            }
        }

        for (auto& child : instance->GetChildren()) {
            CollectInstances(child, viewProj, frustum, outData);
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
            instances.reserve(2000); 
            CollectInstances(workspace, viewProj, frustum, instances);

            // Sorting: Opaque front-to-back (Early-Z), Transparent back-to-front
            std::sort(instances.begin(), instances.end(), [&](const InstanceData& a, const InstanceData& b) {
                bool aTrans = a.color.a < 0.99f;
                bool bTrans = b.color.a < 0.99f;
                if (aTrans != bTrans) return !aTrans; 
                
                float distA = glm::distance2(glm::vec3(a.model[3]), cameraPos);
                float distB = glm::distance2(glm::vec3(b.model[3]), cameraPos);
                return aTrans ? (distA > distB) : (distA < distB);
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
                SDL_PushGPUVertexUniformData(cmd, 0, &lData, sizeof(LightingData));
                SDL_DrawGPUPrimitives(pass, 36, instances.size(), 0, 0);
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
            .num_storage_buffers = 1,
            .num_uniform_buffers = 1
        };

        SDL_GPUShaderCreateInfo fInfo = vInfo;
        fInfo.code_size = fCode.size();
        fInfo.code = fCode.data();
        fInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fInfo.num_storage_buffers = 0;
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

        SDL_GPUVertexAttribute attrs[2];
        attrs[0] = { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 }; // Position
        attrs[1] = { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, sizeof(Vector3) }; // Normal

        SDL_GPUVertexBufferDescription desc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        pInfo.vertex_input_state.num_vertex_attributes = 2;
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
}
