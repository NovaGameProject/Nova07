// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/Instance.hpp"
#include "Common/BrickColors.hpp"
#include "Engine/Common/Signal.hpp"
#include <optional>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace Nova {
    class PhysicsService;

    class BasePart : public Instance {
    public:
        // Properties — direct members, no rfl::Flatten, no Props:: namespace
        CFrame cframe;
        Vector3 size = {4.0f, 1.2f, 2.0f};
        bool anchored = false;
        bool canCollide = true;
        std::optional<Color3> color;
        float transparency = 0.0f;
        int brickColor = 194; // Medium Stone Grey

        SurfaceType topSurface = SurfaceType::Studs;
        SurfaceType bottomSurface = SurfaceType::Inlets;
        SurfaceType leftSurface = SurfaceType::Smooth;
        SurfaceType rightSurface = SurfaceType::Smooth;
        SurfaceType frontSurface = SurfaceType::Smooth;
        SurfaceType backSurface = SurfaceType::Smooth;

        // Signal
        Signal Touched;

        // Jolt Physics linkage
        JPH::BodyID physicsBodyID;
        std::weak_ptr<PhysicsService> registeredService;

        virtual ~BasePart();

        BasePart(std::string name) : Instance(name) {}
        BasePart() : Instance("BasePart") {}

        void InitializePhysics();

        virtual glm::mat4 GetLocalTransform() {
            return cframe.to_mat4();
        }

        virtual glm::mat3 GetRotation() {
            return cframe.rotation;
        }

        virtual glm::vec3 GetSize() {
            return size;
        }

        virtual glm::vec4 GetColor() {
            glm::vec3 rgb;
            if (color.has_value()) {
                rgb = *color;
            } else {
                rgb = BrickColorUtils::ToColor3(brickColor);
            }
            return glm::vec4(rgb, 1.0f - transparency);
        }

        SurfaceType GetSurfaceType(glm::vec3 localNormal) {
            if (localNormal.y > 0.8f) return topSurface;
            if (localNormal.y < -0.8f) return bottomSurface;
            if (localNormal.x > 0.8f) return rightSurface;
            if (localNormal.x < -0.8f) return leftSurface;
            if (localNormal.z > 0.8f) return backSurface;
            if (localNormal.z < -0.8f) return frontSurface;
            return SurfaceType::Smooth;
        }

        void BreakJoints();

        glm::vec3 GetVelocity();
        void SetVelocity(const glm::vec3& velocity);

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;
        void OnPropertyChanged(const std::string& name) override;

        std::string GetClassName() const override { return "BasePart"; }
        std::string GetName() const override { return m_debugName; }
    };
}
