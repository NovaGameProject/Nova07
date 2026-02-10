#pragma once
#include "Engine/TaskScheduler.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_keycode.h>
#include <string>
#include <iostream>

namespace Nova {
    class Window {
    public:
        float mouseDeltaX = 0;
        float mouseDeltaY = 0;
        bool mouseLocked = false;

        Window(std::string title, int width, int height) {
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            }

            // SDL_WINDOW_OPENGL is still used to hint at a hardware-accelerated window
            window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE);
            if (!window) {
                std::cerr << "Window Error: " << SDL_GetError() << std::endl;
            }
        }

        SDL_Window* GetWindow() {
            return window;
        }

        bool PollEvents() {
            SDL_Event event;

            mouseDeltaX = 0; // Reset every frame
            mouseDeltaY = 0;


            // Inside Window::PollEvents()

            while (SDL_PollEvent(&event)) {


                if (event.type == SDL_EVENT_MOUSE_MOTION && mouseLocked) {
                    mouseDeltaX += event.motion.xrel; // Use += to accumulate deltas
                    mouseDeltaY += event.motion.yrel;
                } else if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_ESCAPE) {
                        if (mouseLocked) {
                            SDL_SetWindowRelativeMouseMode(window, false);
                            mouseLocked = false;
                        } else {
                            mouseLocked = true;
                            SDL_SetWindowRelativeMouseMode(window, true);
                        }
                    }
                } else if (event.type == SDL_EVENT_QUIT) return false;
            }
            return true;
        }

        ~Window() {
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

    private:
        SDL_Window* window;
    };
}
