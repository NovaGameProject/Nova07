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
#include <optional>

namespace Nova {
    using Vector3 = glm::vec3;
    using Color3 = glm::vec3;

    struct CFrame {
        Vector3 position = {0,0,0};
        glm::mat3 rotation = glm::mat3(1.0f);

        CFrame() {}
        CFrame(Vector3 pos) : position(pos) {}
        CFrame(Vector3 pos, glm::mat3 rot) : position(pos), rotation(rot) {}

        CFrame operator*(const CFrame& other) const {
            return CFrame(
                position + (rotation * other.position),
                rotation * other.rotation
            );
        }

        CFrame inverse() const {
            glm::mat3 invRot = glm::transpose(rotation);
            return CFrame(invRot * (-position), invRot);
        }

        CFrame to_object_space(const CFrame& world) const {
            return this->inverse() * world;
        }

        CFrame to_world_space(const CFrame& local) const {
            return (*this) * local;
        }

        glm::mat4 to_mat4() const {
            glm::mat4 m(1.0f);
            m[0] = glm::vec4(rotation[0], 0.0f);
            m[1] = glm::vec4(rotation[1], 0.0f);
            m[2] = glm::vec4(rotation[2], 0.0f);
            m[3] = glm::vec4(position, 1.0f);
            return m;
        }

        static CFrame from_mat4(const glm::mat4& m) {
            return CFrame(glm::vec3(m[3]), glm::mat3(m));
        }
    };
}
