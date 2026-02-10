// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <rfl/Flatten.hpp>
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    namespace Props {
        struct ModelProps {
            rfl::Flatten<InstanceProps> base;
        };
    }

    class Model : public Instance {
    public:
        Props::ModelProps props;
        Model() : Instance("Model") {}

        // unsupported by reflection
        // reference to an instance, handled using referents in ROBLOX
        std::weak_ptr<Instance> PrimaryPart;

        NOVA_OBJECT(Model, props)
    };
}
