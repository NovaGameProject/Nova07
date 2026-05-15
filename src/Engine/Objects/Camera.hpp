// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/Instance.hpp"
#include "Common/MathTypes.hpp"

namespace Nova {
    class Camera : public Instance {
    public:
        CFrame cframe;
        CFrame focus;
        CameraType cameraType = CameraType::Fixed;
        float fieldOfView = 70.0f;

        Camera() : Instance("Camera") {}

        glm::mat4 GetViewMatrix() {
            return glm::inverse(cframe.to_mat4());
        }

        std::string GetClassName() const override { return "Camera"; }
        std::string GetName() const override { return m_debugName; }
    };
}
