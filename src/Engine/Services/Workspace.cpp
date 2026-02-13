// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/Workspace.hpp"
#include "Engine/Objects/BasePart.hpp"

namespace Nova {

    static void FindAllPartsRecursive(std::shared_ptr<Instance> inst, std::vector<std::shared_ptr<BasePart>>& out) {
        if (!inst) return;

        if (auto part = std::dynamic_pointer_cast<BasePart>(inst)) {
            out.push_back(part);
        }

        for (auto& child : inst->GetChildren()) {
            FindAllPartsRecursive(child, out);
        }
    }

    void Workspace::RefreshCachedParts() {
        cachedParts.clear();
        FindAllPartsRecursive(shared_from_this(), cachedParts);
    }

}
