#include "Engine/Objects/Instance.hpp"
#include "Engine/Rendering/Renderer.hpp"
#include "Engine/Window.hpp"
#include "Engine/TaskScheduler.hpp"
#include "Engine/Nova.hpp"
#include "Engine/Reflection/LevelLoader.hpp"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Nova::Window window("Nova Engine", 1280, 720);
    Nova::TaskScheduler scheduler;
    bool running = true;


    auto game = std::make_shared<Nova::DataModel>();
    auto workspace = game->GetService<Nova::Workspace>();

    // Load the world from a file
    Nova::LevelLoader::Load("./resources/Places/RobloxHQ.rbxl", game);

    // Apply default lighting if not set by level
    {
        auto lighting = game->GetService<Nova::Lighting>();
        // Check if ClearColor is black or default (usually black 0,0,0 or white 1,1,1)
        // Standard classic Roblox sky was approx rgb(132, 177, 248)
        if (lighting->props.ClearColor.r == 0.0f || (lighting->props.ClearColor.r == 1.0f && lighting->props.ClearColor.g == 1.0f)) {
            lighting->props.ClearColor = { 132/255.0f, 177/255.0f, 248/255.0f }; 
            lighting->props.TopAmbientV9 = { 0.5f, 0.5f, 0.5f };
            lighting->props.BottomAmbientV9 = { 0.2f, 0.2f, 0.2f };
        }
    }

    // MOVE CAMERA OUT OF THE FLOOR (Baseplate is at 0, size 512,8,512 -> y range -4 to 4)
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
            cf.position = glm::vec3(0, 20, 50); // Start back and up
            camera->props.CFrame = Nova::CFrameReflect::from_nova(cf);
            workspace->CurrentCamera = camera; // Link it
            SDL_Log("Camera reset to starting position. PRESS ESC TO LOCK MOUSE AND MOVE.");
        }
    }

    Nova::LevelLoader::PrintInstanceTree(game);

    // Use a unique_ptr so we can explicitly kill the renderer
    // before the window goes out of scope.
    auto renderer = std::make_unique<Nova::Renderer>(window.GetWindow());

    // Outside the scheduler, define some camera state
    float camSpeed = 50.0f;

    scheduler.AddJob({
        .name = "Input",
        .callback = [&](double dt) {
            std::shared_ptr<Nova::Camera> camera = workspace->CurrentCamera;

            // Inside the Input Job callback
            if (camera) {
                const bool* keys = SDL_GetKeyboardState(NULL);
                float speed = 100.0f * (float)dt; // Increased speed for visibility
                float sensitivity = 0.002f;

                // 1. Convert the current reflected data to our math-friendly CFrame
                auto novaCF = camera->props.CFrame.get().to_nova();

                float dx = window.mouseDeltaX;
                float dy = window.mouseDeltaY;

                // 2. YAW: Rotate around World Up (Global Y-axis)
                // Applying this on the LEFT side rotates around the global axis.
                glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), -dx * sensitivity, glm::vec3(0, 1, 0));

                // 3. PITCH: Rotate around Local Right (Local X-axis)
                // Applying this on the RIGHT side rotates around the object's own axis.
                glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), -dy * sensitivity, glm::vec3(1, 0, 0));

                // 4. COMBINE: New Rotation = Yaw * CurrentRotation * Pitch
                novaCF.rotation = glm::mat3(yaw * glm::mat4(novaCF.rotation) * pitch);

                // 5. TRANSLATION (WASD + EQ)
                glm::vec3 forward = -novaCF.rotation[2]; // Forward is -Z in Roblox/GLM
                glm::vec3 right   = novaCF.rotation[0]; // Right is +X

                if (keys[SDL_SCANCODE_W]) novaCF.position += forward * speed;
                if (keys[SDL_SCANCODE_S]) novaCF.position -= forward * speed;
                if (keys[SDL_SCANCODE_A]) novaCF.position -= right * speed;
                if (keys[SDL_SCANCODE_D]) novaCF.position += right * speed;
                if (keys[SDL_SCANCODE_E]) novaCF.position.y += speed; // Fly UP
                if (keys[SDL_SCANCODE_Q]) novaCF.position.y -= speed; // Fly DOWN

                // 6. Push the modified data back into the reflected property
                camera->props.CFrame = Nova::CFrameReflect::from_nova(novaCF);
            }
        },
        .priority = 0,
        .frequency = 0
    });

    scheduler.AddJob({
        .name = "Render",
        .callback = [&](double dt) {
            (void)dt;
            // Get the Workspace from your DataModel
            auto workspace = game->GetService<Nova::Workspace>();
            renderer->RenderFrame(workspace);
        },
        .priority = 100,
        .frequency = 0
    });

    while (running) {
        // 1. OS Events (High Priority, once per frame)
        running = window.PollEvents();

        // 2. Marshalling (Run tasks sent from other threads)
        scheduler.ProcessMainThreadTasks();

        // 3. Engine Step (Physics, Animation, Input Logic, Rendering)
        scheduler.Step();

        // 4. Yield (Optional: prevents 100% CPU usage on some Linux distros)
        // SDL_Delay(1);
    }

    // --- MANUALLY SHUT DOWN IN ORDER ---

    // 1. Kill the renderer first.
    // This calls ~Renderer, waits for GPU, unclaims window, and destroys device.
    renderer.reset();

    // 2. Clear scheduler jobs to release any captured references
    // (This prevents lambdas from holding onto dead pointers)
    scheduler.Clear(); 

    return 0;
    // 3. window goes out of scope last, calling SDL_DestroyWindow and SDL_Quit.
}
