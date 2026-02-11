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
#include "Common/BrickColors.hpp"
#include <rfl/Flatten.hpp>
#include <optional>

namespace Nova {
    namespace Props {
        struct BasePartProps {
            rfl::Flatten<Props::InstanceProps> base;
            rfl::Rename<"CFrame", CFrameReflect> CFrame;

            Vector3Reflect size = {4.0f, 1.2f, 2.0f};
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
        // Direct pointer to props for high-performance access in the renderer
        Props::BasePartProps* basePartProps = nullptr;

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
            if (basePartProps) return basePartProps->size.to_glm();
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
    };
}
