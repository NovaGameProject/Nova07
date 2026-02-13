// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <rfl/Flatten.hpp>
#include <rfl/Rename.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/Instance.hpp"
#include "Common/MathTypes.hpp"

namespace Nova {
    namespace Props {
        struct CameraProps {
            rfl::Flatten<InstanceProps> base;
            CFrameReflect CFrame;
            CFrameReflect Focus;
            rfl::Rename<"CameraType", enum CameraType> CameraType = CameraType::Fixed;
            // float FieldOfView = 70.0f;

        };
    }

    class Camera : public Instance {
    public:
        Props::CameraProps props;

        Camera() : Instance("Camera") {}

        glm::mat4 GetViewMatrix() {
            // The View matrix is the inverse of the Camera's World CFrame
            return glm::inverse(props.CFrame.to_nova().to_mat4());
        }

        NOVA_OBJECT(Camera, props)
    };
}
