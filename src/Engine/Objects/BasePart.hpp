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
#include <rfl/Flatten.hpp>
#include <optional>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace Nova {
    class PhysicsService;

    namespace Props {
        struct BasePartProps {
            rfl::Flatten<Props::InstanceProps> base;
            
            rfl::Rename<"CFrame", CFrameReflect> CFrame;
            rfl::Rename<"Size", Vector3Reflect> Size = Vector3Reflect{4.0f, 1.2f, 2.0f};
            
            bool Anchored = false;
            bool CanCollide = true;
            
            std::optional<Color3Reflect> Color;
            float Transparency = 0.0f;
            int BrickColor = 194; // Medium Stone Grey

            SurfaceType TopSurface = SurfaceType::Studs;
            SurfaceType BottomSurface = SurfaceType::Inlets;
            SurfaceType LeftSurface = SurfaceType::Smooth;
            SurfaceType RightSurface = SurfaceType::Smooth;
            SurfaceType FrontSurface = SurfaceType::Smooth;
            SurfaceType BackSurface = SurfaceType::Smooth;
        };
    }

    class BasePart : public Instance {
    public:
        Signal Touched;
        virtual ~BasePart();

        // Direct pointer to props for high-performance access in the renderer
        Props::BasePartProps* basePartProps = nullptr;

        // Jolt Physics linkage
        JPH::BodyID physicsBodyID;
        std::weak_ptr<PhysicsService> registeredService;

        void InitializePhysics();

        BasePart(std::string name) : Instance(name) {}

        virtual glm::mat4 GetLocalTransform() {
            if (basePartProps) return basePartProps->CFrame.get().to_nova().to_mat4();
            return glm::mat4(1.0f);
        }

        virtual glm::mat3 GetRotation() {
            if (basePartProps) return basePartProps->CFrame.get().to_nova().rotation;
            return glm::mat3(1.0f);
        }

        virtual glm::vec3 GetSize() {
            if (basePartProps) return basePartProps->Size.get().to_glm();
            return glm::vec3(1.0f);
        }

        virtual glm::vec4 GetColor() {
            if (!basePartProps) return glm::vec4(1.0f);

            glm::vec3 rgb;
            if (basePartProps->Color.has_value()) {
                rgb = basePartProps->Color->to_glm();
            } else {
                rgb = BrickColorUtils::ToColor3(basePartProps->BrickColor);
            }
            return glm::vec4(rgb, 1.0f - basePartProps->Transparency);
        }

        SurfaceType GetSurfaceType(glm::vec3 localNormal) {
            if (!basePartProps) return SurfaceType::Smooth;
            
            // Map normal to face
            if (localNormal.y > 0.8f) return basePartProps->TopSurface;
            if (localNormal.y < -0.8f) return basePartProps->BottomSurface;
            if (localNormal.x > 0.8f) return basePartProps->RightSurface;
            if (localNormal.x < -0.8f) return basePartProps->LeftSurface;
            if (localNormal.z > 0.8f) return basePartProps->BackSurface;
            if (localNormal.z < -0.8f) return basePartProps->FrontSurface;
            
            return SurfaceType::Smooth;
        }

        void BreakJoints();

        glm::vec3 GetVelocity();
        void SetVelocity(const glm::vec3& velocity);

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;
        void OnPropertyChanged(const std::string& name) override;
    };
}
