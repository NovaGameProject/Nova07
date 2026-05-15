// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    class Model : public Instance {
    public:
        std::weak_ptr<Instance> PrimaryPart;

        Model() : Instance("Model") {}
        std::string GetClassName() const override { return "Model"; }
        std::string GetName() const override { return m_debugName; }
    };
}
