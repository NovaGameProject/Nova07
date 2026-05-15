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
    std::string connectHost;
    uint16_t connectPort = 27015;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            connectHost = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            connectPort = static_cast<uint16_t>(atoi(argv[++i]));
        }
    }

    Nova::Engine engine;

    if (!connectHost.empty()) {
        // Client mode: connect to server, no local level loading
        if (!engine.InitializeClient(connectHost, connectPort)) {
            LOG_ERR("Client", "Failed to connect to %s:%u", connectHost.c_str(), connectPort);
            return 1;
        }
        // Client waits for server to send world state - no LoadLevel() here
    } else {
        // PlaySolo mode: local simulation
        if (!engine.Initialize("Nova Engine", 1280, 720)) {
            return 1;
        }

        engine.LoadLevel("./resources/Places/RobloxHQ.rbxl");

        // Run test scripts in PlaySolo mode
        auto scriptContext = engine.GetDataModel()->GetService<Nova::ScriptContext>();

        scriptContext->Execute("print('Hello from Luau!')");
        scriptContext->Execute("print('Game name: ' .. game.Name)");
        scriptContext->Execute("print('Workspace name: ' .. workspace.Name)");

        scriptContext->Execute(R"(
            local touched = false
            local p = Instance.new("Part")
            p.Name = "MyLuauPart"
            p.Parent = workspace
            p.Position = Vector3.new(100, 250, 150)
            p.Size = Vector3.new(2, 1, 2)
            print("Created part: " .. p.Name)

            p.Touched:Connect(function(other)
                print("Part touched by " .. other.Name)
                if touched then return end
                touched = true

                local explosion = Instance.new("Explosion")
                explosion.Position = p.Position
                explosion.BlastRadius = 100
                explosion.BlastPressure = 10000
                explosion.Parent = workspace
            end)
        )");
    }

    engine.Run();

    return 0;
}
