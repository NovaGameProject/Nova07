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

        // reflect-cpp unsupported:
        std::weak_ptr<Camera> CurrentCamera;
        std::weak_ptr<Instance> PrimaryPart;

        Props::WorkspaceProps props;
        NOVA_OBJECT(Workspace, props)

        Workspace() : Instance("Workspace") {}
    };

}
