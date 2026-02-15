// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Engine.hpp"
#include "Engine/Nova.hpp"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    Nova::Engine engine;
    if (!engine.Initialize("Nova Engine", 1280, 720)) {
        return 1;
    }

    engine.LoadLevel("./resources/Places/HappyHomeInRobloxia.rbxl");

    auto scriptContext = engine.GetDataModel()->GetService<Nova::ScriptContext>();

    // Test basic print and globals
    scriptContext->Execute("print('Hello from Luau!')");
    scriptContext->Execute("print('Game name: ' .. game.Name)");
    scriptContext->Execute("print('Workspace name: ' .. workspace.Name)");

    // Test dynamic property access and children
    scriptContext->Execute(R"(
        local p = Instance.new("Part")
        p.Name = "MyLuauPart"
        p.Parent = workspace
        p.Position = Vector3.new(0, 100, 0)
        p.Size = Vector3.new(2, 1, 2)
        print("Created part: " .. p.Name)
        print("Part Parent: " .. p.Parent.Name)

        -- Test signal connection
        p.Touched:Connect(function(other)
            print("Part touched by " .. other.Name)

            local explosion = Instance.new("Explosion")
            explosion.Position = p.Position
            explosion.BlastRadius = 10000
            explosion.BlastPressure = 100000000
            print(p.Position, explosion.Position)
            explosion.Parent = workspace
        end)
        print("Connected to Touched signal")
    )");

    engine.Run();

    return 0;
}
