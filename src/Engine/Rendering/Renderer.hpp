#pragma once
#include "Engine/Objects/Instance.hpp"
#include "Engine/Services/Workspace.hpp"
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Nova {
    // Forward declare vertex to keep header light
    struct Vertex;

    struct SceneUniforms {
        glm::mat4 mvp;
    };

    struct Framebuffer {
        SDL_GPUTexture* depthTexture = nullptr;
        uint32_t width = 0, height = 0;

        void Refresh(SDL_GPUDevice* device, uint32_t w, uint32_t h);
        void Cleanup(SDL_GPUDevice* device);
    };

    class Renderer {
    public:
        Renderer(SDL_Window* window);
        ~Renderer();

        void RenderFrame(std::shared_ptr<Workspace> root);
        void SubmitInstance(std::shared_ptr<Instance> instance,
            const glm::mat4& parentTransform,
            const glm::mat4& viewProj,
            SDL_GPUCommandBuffer* cmd,
            SDL_GPURenderPass* pass);

    private:
        SDL_GPUDevice* device;
        SDL_Window* window;
        SDL_GPUGraphicsPipeline* basePipeline;
        SDL_GPUBuffer* cubeBuffer;
        Framebuffer fb;

        std::vector<uint8_t> LoadSPIRV(const std::string& path);
        void InitPipelines();
        void CreateCubeResources();
    };
}
