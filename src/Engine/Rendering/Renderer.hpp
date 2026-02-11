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
        glm::vec4 lightDir; // w is intensity or unused
    };

    class Renderer {
    public:
        Renderer(SDL_Window* window);
        ~Renderer();

        void RenderFrame(std::shared_ptr<Workspace> root);

    private:
        struct InstanceData {
            glm::mat4 mvp;
            glm::mat4 model;
            glm::vec4 color;
            // Pack 6 SurfaceType enums (int32 each)
            int32_t surfaces[6]; // Order: Z+, Z-, X-, X+, Y+, Y- (matching Geometry.hpp)
            float padding[2];    // Ensure 16-byte alignment
        };

        void CollectInstances(std::shared_ptr<Instance> instance,
            const glm::mat4& viewProj,
            const Frustum& frustum,
            std::vector<InstanceData>& outData);

        SDL_GPUDevice* device;
        SDL_Window* window;
        SDL_GPUGraphicsPipeline* basePipeline;
        SDL_GPUBuffer* cubeBuffer;
        SDL_GPUBuffer* instanceBuffer;
        SDL_GPUTransferBuffer* instanceTransferBuffer = nullptr;

        SDL_GPUTexture* surfaceTexture = nullptr;
        SDL_GPUSampler* surfaceSampler = nullptr;

        Framebuffer fb;

        std::vector<uint8_t> LoadSPIRV(const std::string& path);
        void InitPipelines();
        void CreateCubeResources();
    };
}
