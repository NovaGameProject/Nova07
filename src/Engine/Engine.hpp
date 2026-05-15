// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/TaskScheduler.hpp"
#include "Engine/Services/DataModel.hpp"
#include <memory>
#include <string>

namespace Nova {
    class Window;
    class Renderer;

    class Engine {
    public:
        enum class Mode {
            PlaySolo,   // Local simulation, local rendering, no network
            Client,     // No simulation, renders replicated state, sends input
            Server,     // Full simulation, no rendering, replicates to clients
        };

        Engine();
        ~Engine();

        // PlaySolo: window + renderer + full simulation
        bool Initialize(const std::string& title, int width, int height);

        // Server: headless, full simulation, network server
        bool InitializeServer(uint16_t port);

        // Client: window + renderer, no simulation, network client
        bool InitializeClient(const std::string& host, uint16_t port);

        void LoadLevel(const std::string& path);
        void Run();
        void Shutdown();

        std::shared_ptr<DataModel> GetDataModel() const { return dataModel; }
        Mode GetMode() const { return mode; }
        bool IsHeadless() const { return mode == Mode::Server; }

    private:
        void SetupDefaultLighting();
        void SetupJobs();
        bool InternalInitialize(const std::string& title, int width, int height);

        std::unique_ptr<Window> window;
        std::unique_ptr<TaskScheduler> scheduler;
        std::unique_ptr<Renderer> renderer;
        std::shared_ptr<DataModel> dataModel;

        Mode mode = Mode::PlaySolo;
        bool running = false;
    };
}
