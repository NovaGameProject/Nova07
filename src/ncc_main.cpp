// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Engine.hpp"
#include "Engine/Nova.hpp"
#include "Common/Log.hpp"
#include <cstring>
#include <string>

int main(int argc, char* argv[]) {
    std::string level = "./resources/Places/RobloxHQ.rbxl";
    uint16_t port = 27015;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            level = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(atoi(argv[++i]));
        }
    }

    Nova::Engine engine;
    if (!engine.InitializeServer(port)) {
        LOG_ERR("NCCService", "Failed to start server on port %u", port);
        return 1;
    }

    engine.LoadLevel(level);

    LOG_INF("NCCService", "Server running on port %u. Press Ctrl+C to stop.", port);
    engine.Run();

    return 0;
}
