// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Engine.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Engine/Nova.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <tracy/Tracy.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Nova {
    Engine::Engine() {
        RegisterClasses();
        dataModel = std::make_shared<DataModel>();
        scheduler = std::make_unique<TaskScheduler>();
    }

    Engine::~Engine() {
        Shutdown();
    }

    bool Engine::Initialize(const std::string& title, int width, int height) {
        window = std::make_unique<Window>(title, width, height);
        if (!window->GetWindow()) {
            return false;
        }

        renderer = std::make_unique<Renderer>(window->GetWindow());

        SetupJobs();

        return true;
    }

    void Engine::LoadLevel(const std::string& path) {
        LevelLoader::Load(path, dataModel);

        auto physics = dataModel->GetService<PhysicsService>();
        physics->Start();

        auto scriptContext = dataModel->GetService<ScriptContext>();
        scriptContext->SetDataModel(dataModel);

        SetupDefaultLighting();
        LevelLoader::PrintInstanceTree(dataModel);
    }

    void Engine::SetupDefaultLighting() {
        auto lighting = dataModel->GetService<Lighting>();
        if (lighting->props.ClearColor.r == 0.0f || (lighting->props.ClearColor.r == 1.0f && lighting->props.ClearColor.g == 1.0f)) {
            lighting->props.ClearColor = { 132/255.0f, 177/255.0f, 248/255.0f };
            lighting->props.TopAmbientV9 = { 0.5f, 0.5f, 0.5f };
            lighting->props.BottomAmbientV9 = { 0.2f, 0.2f, 0.2f };
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
            SDL_Log("Added default Sky instance.");
        }
    }

    void Engine::SetupJobs() {
        auto workspace = dataModel->GetService<Workspace>();
        auto physics = dataModel->GetService<PhysicsService>();

        scheduler->AddJob({
            .name = "PhysicsSync",
            .callback = [physics](double dt) {
                physics->Step(dt);
            },
            .priority = 5,
            .frequency = 0
        });

        scheduler->AddJob({
            .name = "Input",
            .callback = [this, workspace](double dt) {
                std::shared_ptr<Camera> camera = workspace->CurrentCamera;
                if (camera) {
                    const bool* keys = SDL_GetKeyboardState(NULL);
                    float speed = 100.0f * (float)dt;
                    float sensitivity = 0.002f;
                    auto novaCF = camera->props.CFrame.to_nova();
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
                    camera->props.CFrame = CFrameReflect::from_nova(novaCF);
                }
            },
            .priority = 10,
            .frequency = 0
        });

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

    void Engine::Run() {
        running = true;
        while (running) {
            running = window->PollEvents();
            scheduler->ProcessMainThreadTasks();
            scheduler->Step();

            FrameMark;
        }
    }

    void Engine::Shutdown() {
        if (renderer) {
            renderer.reset();
        }
        if (scheduler) {
            scheduler->Clear();
        }
        running = false;
    }
}
