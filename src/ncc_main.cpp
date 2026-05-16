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

    // Run test scripts on server
    auto scriptContext = engine.GetDataModel()->GetService<Nova::ScriptContext>();

    scriptContext->Execute("print('Server: Hello from Luau!')");
    scriptContext->Execute("print('Server: Game name: ' .. game.Name)");
    scriptContext->Execute("print('Server: Workspace name: ' .. workspace.Name)");

    // Explosion test script
    scriptContext->Execute(R"(
        local touched = false
        local p = Instance.new("Part")
        p.Name = "ExplosionTestPart"
        p.Parent = workspace
        p.Position = Vector3.new(100, 1000, 150)
        p.Size = Vector3.new(2, 1, 2)
        print("Server: Created part: " .. p.Name)

        p.Touched:Connect(function(other)
            print("Server: Part touched by " .. other.Name)
            if touched then return end
            touched = true

            local explosion = Instance.new("Explosion")
            explosion.Position = p.Position
            explosion.BlastRadius = 1000
            explosion.BlastPressure = 10000
            explosion.Parent = workspace
            print("Server: Explosion created!")
        end)
    )");

    LOG_INF("NCCService", "Server running on port %u. Press Ctrl+C to stop.", port);
    engine.Run();

    return 0;
}
