// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Common/MathTypes.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace Nova {
    class BasePart;

    struct Assembly {
        JPH::BodyID bodyID;
        BasePart* rootPart = nullptr;
        std::vector<std::weak_ptr<BasePart>> parts;
        std::unordered_map<BasePart*, CFrame> relativeTransforms;
        bool isStatic = false;

        // Use a set to avoid duplicates and allow efficient removal
        std::unordered_set<JPH::Constraint*> attachedConstraints;
    };
}
