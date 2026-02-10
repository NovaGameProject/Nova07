// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Objects/Instance.hpp"
#include <rfl/Flatten.hpp>

namespace Nova {
    namespace Props {
        struct BasePartProps {
            rfl::Flatten<Props::InstanceProps> base;
            rfl::Rename<"CFrame", CFrameReflect> CFrame;

            Vector3Reflect size = {4.0f, 1.2f, 2.0f};
            bool Anchored = false;
            bool CanCollide = true;
            float Transparency = 0.0f;
            int BrickColor = 194; // Medium Stone Grey
        };
    }

    class BasePart : public Instance {
    public:
        Props::BasePartProps props;
        BasePart(std::string name) : Instance(name) {}

        NOVA_OBJECT(BasePart, props)
    };
}
