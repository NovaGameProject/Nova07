// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <rfl.hpp>

using Vector3 = glm::vec3;
using Color3 = glm::vec3;

struct CFrame {
    Vector3 position = {0,0,0};
    glm::mat3 rotation = glm::mat3(1.0f);

    // This converts your CFrame into the 4x4 matrix used for Rendering/Physics
    glm::mat4 to_mat4() const {
        glm::mat4 m = glm::mat4(rotation); // Fill the top-left 3x3
        m[3] = glm::vec4(position, 1.0f);   // Set the translation column
        return m;
    }

    auto reflect() const {
        return std::array<float, 12>{
            rotation[0][0], rotation[0][1], rotation[0][2],
            rotation[1][0], rotation[1][1], rotation[1][2],
            rotation[2][0], rotation[2][1], rotation[2][2],
            position.x, position.y, position.z
        };
    }
};


namespace Nova {
    // These are "dumb" versions for reflection only
    struct Vector3Reflect {
        float x, y, z;
        Vector3 to_glm() const { return Vector3(x, y, z); }

        static Vector3Reflect from_glm(const Vector3& v) {
            return {v.x, v.y, v.z};
        }
    };

    struct Color3Reflect {
        float r = 1.0f, g = 1.0f, b = 1.0f;

        // Conversion to your glm-based Color3
        Color3 to_glm() const { return Color3(r, g, b); }

        static Color3Reflect from_glm(const Color3& c) {
            return { c.r, c.g, c.b };
        }
    };


    struct CFrameReflect {
        float x, y, z;
        float r00, r01, r02, r10, r11, r12, r20, r21, r22;

        CFrame to_nova() const {
            CFrame cf;
            cf.position = {x, y, z};

            // GLM mat3 columns are (col0, col1, col2)
            // We map the XML Row values to the GLM Columns
            cf.rotation[0] = glm::vec3(r00, r10, r20); // Column 0
            cf.rotation[1] = glm::vec3(r01, r11, r21); // Column 1
            cf.rotation[2] = glm::vec3(r02, r12, r22); // Column 2

            return cf;
        }

        static CFrameReflect from_nova(const CFrame& cf) {
            return {
                cf.position.x, cf.position.y, cf.position.z,
                cf.rotation[0][0], cf.rotation[1][0], cf.rotation[2][0], // Row 0 (R00, R01, R02)
                cf.rotation[0][1], cf.rotation[1][1], cf.rotation[2][1], // Row 1 (R10, R11, R12)
                cf.rotation[0][2], cf.rotation[1][2], cf.rotation[2][2]  // Row 2 (R20, R21, R22)
            };
        }
    };
}
