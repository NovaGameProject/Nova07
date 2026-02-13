// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include "Engine/Objects/Camera.hpp"
#include <vector>
#include <memory>

namespace Nova {

    class BasePart;

    namespace Props {
        struct WorkspaceProps {
            rfl::Flatten<InstanceProps> base;
            float FallenPartsDestroyHeight = -500.0f;
        };
    }

    class Workspace : public Instance {
    public:
        Props::WorkspaceProps props;
        NOVA_OBJECT(Workspace, props)

        Workspace() : Instance("Workspace") {}

        std::shared_ptr<Camera> CurrentCamera;

        // Optimization: Flattened list of physical parts for the renderer/physics sync
        std::vector<std::shared_ptr<BasePart>> cachedParts;

        void RefreshCachedParts();
    };

}
