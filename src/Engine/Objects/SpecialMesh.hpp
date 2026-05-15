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
#include <string>

namespace Nova {
    class SpecialMesh : public Instance {
    public:
        std::string MeshId = "";
        std::string TextureId = "";
        Vector3 Scale = {1, 1, 1};

        SpecialMesh() : Instance("SpecialMesh") {}
        std::string GetClassName() const override { return "SpecialMesh"; }
        std::string GetName() const override { return m_debugName; }
    };
}
