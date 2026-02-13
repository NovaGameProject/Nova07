// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/Window.hpp"
#include "Engine/TaskScheduler.hpp"
#include "Engine/Rendering/Renderer.hpp"
#include "Engine/Services/DataModel.hpp"
#include <memory>
#include <string>

namespace Nova {
    class Engine {
    public:
        Engine();
        ~Engine();

        bool Initialize(const std::string& title, int width, int height);
        void LoadLevel(const std::string& path);
        void Run();
        void Shutdown();

        std::shared_ptr<DataModel> GetDataModel() const { return dataModel; }

    private:
        void SetupDefaultLighting();
        void SetupJobs();

        std::unique_ptr<Window> window;
        std::unique_ptr<TaskScheduler> scheduler;
        std::unique_ptr<Renderer> renderer;
        std::shared_ptr<DataModel> dataModel;

        bool running = false;
    };
}
