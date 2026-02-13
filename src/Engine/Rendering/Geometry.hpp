// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <vector>
#include <SDL3/SDL_gpu.h>

#include "Common/MathTypes.hpp"

namespace Nova {
    struct Vertex {
        Vector3 pos;
        Vector3 normal;
        glm::vec2 uv;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    class Geometry {
    public:
        static Mesh CreateCube() {
            Mesh mesh;
            // 36 vertices for flat shading (6 faces * 2 triangles * 3 verts)
            // Surfaces order matches InstanceData: Z+, Z-, X-, X+, Y+, Y-
            mesh.vertices = {
                // Front face (Z+) - Index 0
                {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 1}}, {{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 1}}, {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 0}},
                {{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 0}}, {{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 0}}, {{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 1}},
                
                // Back face (Z-) - Index 1
                {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}}, {{-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 0}}, {{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
                {{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 0}}, {{ 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}}, {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
                
                // Left face (X-) - Index 2
                {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 0}}, {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 0}}, {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
                {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}}, {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 1}}, {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 0}},
                
                // Right face (X+) - Index 3
                {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 0}}, {{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 1}}, {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
                {{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}}, {{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 0}}, {{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 0}},
                
                // Top face (Y+) - Index 4
                {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 0}}, {{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 1}}, {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 1}},
                {{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 1}}, {{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 0}}, {{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
                
                // Bottom face (Y-) - Index 5
                {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}}, {{ 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}}, {{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 0}},
                {{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 0}}, {{-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 0}}, {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}}
            };
            return mesh;
        }
    };
}
