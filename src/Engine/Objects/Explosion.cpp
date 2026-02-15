// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Explosion.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"

namespace Nova {
    Explosion::Explosion() : Instance("Explosion") {}

    void Explosion::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);
        
        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                if (auto physics = dm->GetService<PhysicsService>()) {
                    physics->BreakJointsInRadius(props.Position.get().to_glm(), props.BlastRadius);
                    // In a real implementation, we'd also apply impulsed to bodies here.
                }
            }
        }
    }
}
