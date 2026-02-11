#include "Engine/Objects/Instance.hpp"
#include "Engine/Rendering/Renderer.hpp"
#include "Engine/Window.hpp"
#include "Engine/TaskScheduler.hpp"
#include "Engine/Nova.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include <SDL3_image/SDL_image.h>
#include <tracy/Tracy.hpp>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Nova::Window window("Nova Engine", 1280, 720);
    Nova::TaskScheduler scheduler;
    bool running = true;


    auto game = std::make_shared<Nova::DataModel>();
    auto workspace = game->GetService<Nova::Workspace>();
    auto physics = game->GetService<Nova::PhysicsService>();

    // Load the world from a file
    Nova::LevelLoader::Load("./resources/Places/HappyHomeInRobloxia.rbxl", game);

    physics->Start();

    // Apply default lighting if not set by level
    {
        auto lighting = game->GetService<Nova::Lighting>();
        if (lighting->props.ClearColor.r == 0.0f || (lighting->props.ClearColor.r == 1.0f && lighting->props.ClearColor.g == 1.0f)) {
            lighting->props.ClearColor = { 132/255.0f, 177/255.0f, 248/255.0f };
            lighting->props.TopAmbientV9 = { 0.5f, 0.5f, 0.5f };
            lighting->props.BottomAmbientV9 = { 0.2f, 0.2f, 0.2f };
        }
    }

    // MOVE CAMERA OUT OF THE FLOOR
    {
        auto workspace = game->GetService<Nova::Workspace>();
        auto findCamera = [](auto& self, std::shared_ptr<Nova::Instance> inst) -> std::shared_ptr<Nova::Camera> {
            if (auto c = std::dynamic_pointer_cast<Nova::Camera>(inst)) return c;
            for (auto& child : inst->GetChildren()) {
                if (auto found = self(self, child)) return found;
            }
            return nullptr;
        };
        if (auto camera = findCamera(findCamera, workspace)) {
            auto cf = camera->props.CFrame.get().to_nova();
            cf.position = glm::vec3(0, 20, 50);
            camera->props.CFrame = Nova::CFrameReflect::from_nova(cf);
            workspace->CurrentCamera = camera;
            SDL_Log("Camera reset. ESC TO LOCK MOUSE.");
        }
    }

    Nova::LevelLoader::PrintInstanceTree(game);

    auto renderer = std::make_unique<Nova::Renderer>(window.GetWindow());

    scheduler.AddJob({
        .name = "PhysicsSync",
        .callback = [&](double dt) {
            physics->Step(dt);
        },
        .priority = 5,
        .frequency = 0
    });

    scheduler.AddJob({
        .name = "Input",
        .callback = [&](double dt) {
            std::shared_ptr<Nova::Camera> camera = workspace->CurrentCamera;
            if (camera) {
                const bool* keys = SDL_GetKeyboardState(NULL);
                float speed = 100.0f * (float)dt;
                float sensitivity = 0.002f;
                auto novaCF = camera->props.CFrame.get().to_nova();
                float dx = window.mouseDeltaX;
                float dy = window.mouseDeltaY;
                glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), -dx * sensitivity, glm::vec3(0, 1, 0));
                glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), -dy * sensitivity, glm::vec3(1, 0, 0));
                novaCF.rotation = glm::mat3(yaw * glm::mat4(novaCF.rotation) * pitch);
                glm::vec3 forward = -novaCF.rotation[2];
                glm::vec3 right   = novaCF.rotation[0];
                if (keys[SDL_SCANCODE_W]) novaCF.position += forward * speed;
                if (keys[SDL_SCANCODE_S]) novaCF.position -= forward * speed;
                if (keys[SDL_SCANCODE_A]) novaCF.position -= right * speed;
                if (keys[SDL_SCANCODE_D]) novaCF.position += right * speed;
                if (keys[SDL_SCANCODE_E]) novaCF.position.y += speed;
                if (keys[SDL_SCANCODE_Q]) novaCF.position.y -= speed;
                camera->props.CFrame = Nova::CFrameReflect::from_nova(novaCF);

            }
        },
        .priority = 10,
        .frequency = 0
    });

    scheduler.AddJob({
        .name = "Render",
        .callback = [&](double dt) {
            (void)dt;
            renderer->RenderFrame(workspace);

        },
        .priority = 100,
        .frequency = 0
    });

    while (running) {
        running = window.PollEvents();
        scheduler.ProcessMainThreadTasks();
        scheduler.Step();

        FrameMark;
    }

    renderer.reset();
    scheduler.Clear();
    return 0;
}
