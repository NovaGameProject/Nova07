// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Engine.hpp"
#include "Engine/Window.hpp"
#include "Engine/Rendering/Renderer.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Engine/Nova.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Common/Log.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <tracy/Tracy.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>
#include <chrono>

namespace Nova {
    Engine::Engine() {
        RegisterClasses();
        dataModel = std::make_shared<DataModel>();
        scheduler = std::make_unique<TaskScheduler>();
    }

    Engine::~Engine() {
        Shutdown();
    }

    bool Engine::InternalInitialize(const std::string& title, int width, int height) {
        if (mode != Mode::Server) {
            window = std::make_unique<Window>(title, width, height);
            if (!window->GetWindow()) {
                return false;
            }
            renderer = std::make_unique<Renderer>(window->GetWindow());
        }

        SetupJobs();
        return true;
    }

    bool Engine::Initialize(const std::string& title, int width, int height) {
        mode = Mode::PlaySolo;
        return InternalInitialize(title, width, height);
    }

    bool Engine::InitializeServer(uint16_t port) {
        mode = Mode::Server;
        LOG_INF("Engine", "Starting as server on port %u.", port);

        if (!InternalInitialize("", 0, 0)) {
            return false;
        }

        // Start the network server
        auto network = dataModel->GetService<NetworkService>();
        if (!network->StartServer(port)) {
            LOG_ERR("Engine", "Failed to start server on port %u", port);
            return false;
        }

        return true;
    }

    bool Engine::InitializeClient(const std::string& host, uint16_t port) {
        mode = Mode::Client;
        LOG_INF("Engine", "Starting as client, connecting to %s:%u.", host.c_str(), port);

        if (!InternalInitialize("Nova Engine", 1280, 720)) {
            return false;
        }

        // Set up default lighting and sky for the client
        SetupDefaultLighting();

        // Make sure Workspace has a Camera
        auto workspace = dataModel->GetService<Workspace>();
        if (!workspace->CurrentCamera) {
            auto camera = std::make_shared<Camera>();
            camera->SetParent(workspace);
            workspace->CurrentCamera = camera;
        }

        // Connect to server
        auto network = dataModel->GetService<NetworkService>();
        if (!network->ConnectToServer(host, port)) {
            LOG_ERR("Engine", "Failed to connect to %s:%u", host.c_str(), port);
            return false;
        }

        return true;
    }

    void Engine::LoadLevel(const std::string& path) {
        LevelLoader::Load(path, dataModel);

        // Physics and scripts only run on Server and PlaySolo
        if (mode != Mode::Client) {
            auto physics = dataModel->GetService<PhysicsService>();
            physics->Start();

            auto scriptContext = dataModel->GetService<ScriptContext>();
            scriptContext->SetDataModel(dataModel);
        }

        SetupDefaultLighting();
    }

    void Engine::SetupDefaultLighting() {
        auto lighting = dataModel->GetService<Lighting>();
        if (lighting->ClearColor.r == 0.0f || (lighting->ClearColor.r == 1.0f && lighting->ClearColor.g == 1.0f)) {
            lighting->ClearColor = { 132/255.0f, 177/255.0f, 248/255.0f };
            lighting->TopAmbientV9 = { 0.5f, 0.5f, 0.5f };
            lighting->BottomAmbientV9 = { 0.2f, 0.2f, 0.2f };
        }

        // Add default Sky if none exists
        bool hasSky = false;
        for (auto& child : lighting->GetChildren()) {
            if (std::dynamic_pointer_cast<Sky>(child)) {
                hasSky = true;
                break;
            }
        }
        if (!hasSky) {
            auto sky = std::make_shared<Sky>();
            sky->SetParent(lighting);
            LOG_INF("Engine", "Added default Sky instance.");
        }
    }

    void Engine::SetupJobs() {
        auto workspace = dataModel->GetService<Workspace>();
        auto physics = dataModel->GetService<PhysicsService>();
        auto network = dataModel->GetService<NetworkService>();

        // Physics: only on Server and PlaySolo
        if (mode == Mode::PlaySolo || mode == Mode::Server) {
            scheduler->AddJob({
                .name = "PhysicsSync",
                .callback = [physics](double dt) {
                    physics->Step(dt);
                    physics->UpdateHumanoids(dt);
                },
                .priority = 5,
                .frequency = 0
            });
        }

        // Network: on Client and Server
        if (mode == Mode::Client || mode == Mode::Server) {
            scheduler->AddJob({
                .name = "NetworkTick",
                .callback = [network](double dt) {
                    network->Tick(dt);
                },
                .priority = 50,
                .frequency = 0
            });
        }

        // Input: on PlaySolo and Client (has window)
        if (mode == Mode::PlaySolo || mode == Mode::Client) {
            scheduler->AddJob({
                .name = "Input",
                .callback = [this, workspace](double dt) {
                    std::shared_ptr<Camera> camera = workspace->CurrentCamera;
                    if (camera) {
                        const bool* keys = SDL_GetKeyboardState(NULL);
                        float speed = 100.0f * (float)dt;
                        float sensitivity = 0.002f;
                        auto novaCF = camera->cframe;
                        float dx = window->mouseDeltaX;
                        float dy = window->mouseDeltaY;
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
                        camera->cframe = novaCF;
                    }
                },
                .priority = 10,
                .frequency = 0
            });
        }

        // Render: on PlaySolo and Client (has window)
        if (mode == Mode::PlaySolo || mode == Mode::Client) {
            scheduler->AddJob({
                .name = "Render",
                .callback = [this, workspace](double dt) {
                    (void)dt;
                    renderer->RenderFrame(workspace);
                },
                .priority = 100,
                .frequency = 0
            });
        }
    }

    void Engine::Run() {
        running = true;
        while (running) {
            if (mode != Mode::Server) {
                running = window->PollEvents();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            scheduler->ProcessMainThreadTasks();
            scheduler->Step();

            FrameMark;
        }
    }

    void Engine::Shutdown() {
        if (renderer) {
            renderer.reset();
        }
        if (window) {
            window.reset();
        }
        if (scheduler) {
            scheduler->Clear();
        }
        running = false;
    }
}
