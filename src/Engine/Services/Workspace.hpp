// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Nova.hpp"
#include <memory>

namespace Nova {

    namespace Props {
        struct WorkspaceProps {
            rfl::Flatten<InstanceProps> base;
        };
    }

    class Workspace : public Instance {
    public:

        float Gravity = 196.2f;

        // The active camera, usually resolved from the RBXL file or created on demand
        std::shared_ptr<Camera> CurrentCamera;
        
        std::weak_ptr<Instance> PrimaryPart;

        // High-performance cache for the renderer
        std::vector<std::shared_ptr<BasePart>> cachedParts;

        void RefreshCachedParts() {
            cachedParts.clear();
            auto findParts = [&](auto& self, std::shared_ptr<Instance> inst) -> void {
                if (auto bp = std::dynamic_pointer_cast<BasePart>(inst)) {
                    cachedParts.push_back(bp);
                }
                for (auto& child : inst->GetChildren()) {
                    self(self, child);
                }
            };
            findParts(findParts, shared_from_this());
        }

        Props::WorkspaceProps props;
        NOVA_OBJECT(Workspace, props)

        Workspace() : Instance("Workspace") {
            props.base.get().Name = "Workspace";
        }

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override {
            Instance::OnAncestorChanged(instance, newParent);
            RefreshCachedParts();
        }
    };

}
