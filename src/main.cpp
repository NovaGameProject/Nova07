// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Nova.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include <iostream>

int main() {
    using namespace Nova;

    // 1. Create the root
    auto game = std::make_shared<DataModel>();

    // 2. Get Services
    auto workspace = game->GetService<Workspace>();
    auto lighting = game->GetService<Lighting>();

    // 3. Create a Part
    auto brick = std::make_shared<Part>();
    brick->props.base.get().BrickColor = 21; // Bright Red
    brick->props.base.get().base.get().Name = "MyFirstBrick";

    // 4. Parent it
    brick->SetParent(workspace);

    // 5. Debug Print
    std::cout << "Successfully created " << brick->props.base.get().base.get().Name.value()
                  << " inside " << brick->GetName() << std::endl;

    LevelLoader::Load("resources/test.rbxl", game);
    LevelLoader::PrintInstanceTree(game);
    return 0;
}
