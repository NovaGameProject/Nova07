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

    class Workspace : public Instance {
    public:
        float FallenPartsDestroyHeight = -500.0f;

        std::shared_ptr<Camera> CurrentCamera;
        std::vector<std::shared_ptr<BasePart>> cachedParts;

        Workspace() : Instance("Workspace") {}

        void RefreshCachedParts();

        std::string GetClassName() const override { return "Workspace"; }
        std::string GetName() const override { return m_debugName; }
    };
}
