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
#include <string>

namespace Nova {
    namespace Props {
        struct MeshProps {
            rfl::Flatten<Props::InstanceProps> base;
            std::string MeshId = "";
            std::string TextureId = "";
            Vector3Reflect Scale = {1, 1, 1};
        };
    }

    class SpecialMesh : public Instance {
    public:
        Props::MeshProps props;
        SpecialMesh() : Instance("SpecialMesh") {}

        NOVA_OBJECT(SpecialMesh, props)
    };
}
