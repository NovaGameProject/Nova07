#include "Engine/Renderer.hpp"
#include "Engine/Window.hpp"
#include "Engine/TaskScheduler.hpp"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Nova::Window window("Nova Engine", 1280, 720);
    Nova::TaskScheduler scheduler;
    bool running = true;

    // Use a unique_ptr so we can explicitly kill the renderer
    // before the window goes out of scope.
    auto renderer = std::make_unique<Nova::Renderer>(window.GetWindow());

    scheduler.AddJob({
        .name = "Input",
        .callback = [&](double dt) {
            (void)dt;
            if (!window.PollEvents()) running = false;
            scheduler.ProcessMainThreadTasks();
        },
        .priority = 0,
        .frequency = 0
    });

    scheduler.AddJob({
        .name = "Render",
        .callback = [&](double dt) {
            (void)dt;
            if (renderer) renderer->RenderFrame();
        },
        .priority = 100,
        .frequency = 0
    });

    while (running) {
        scheduler.Step();
    }

    // --- MANUALLY SHUT DOWN IN ORDER ---

    // 1. Kill the renderer first.
    // This calls ~Renderer, waits for GPU, unclaims window, and destroys device.
    renderer.reset();

    // 2. Clear scheduler jobs to release any captured references
    // (This prevents lambdas from holding onto dead pointers)
    // scheduler.Clear(); // If you add a clear method, or just let it die.

    return 0;
    // 3. window goes out of scope last, calling SDL_DestroyWindow and SDL_Quit.
}
