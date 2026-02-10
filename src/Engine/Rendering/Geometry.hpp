#pragma once
#include <vector>
#include <SDL3/SDL_gpu.h>

#include "Common/MathTypes.hpp"

namespace Nova {
    struct Vertex {
        Vector3 pos;
        // Future: Vector3 normal; Vector2 uv;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices; // Optional, but better for performance
    };

    class Geometry {
    public:
        static Mesh CreateCube() {
            Mesh mesh;
            // Define the 8 corners of a 1x1x1 cube
            // We use 36 vertices for a "flat shaded" look (6 faces * 2 triangles * 3 verts)
            mesh.vertices = {
                // Front face (Z+)
                {{-0.5f, -0.5f,  0.5f}}, {{ 0.5f, -0.5f,  0.5f}}, {{ 0.5f,  0.5f,  0.5f}},
                {{ 0.5f,  0.5f,  0.5f}}, {{-0.5f,  0.5f,  0.5f}}, {{-0.5f, -0.5f,  0.5f}},
                // Back face (Z-)
                {{-0.5f, -0.5f, -0.5f}}, {{-0.5f,  0.5f, -0.5f}}, {{ 0.5f,  0.5f, -0.5f}},
                {{ 0.5f,  0.5f, -0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f, -0.5f}},
                // Left face (X-)
                {{-0.5f,  0.5f,  0.5f}}, {{-0.5f,  0.5f, -0.5f}}, {{-0.5f, -0.5f, -0.5f}},
                {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f,  0.5f}}, {{-0.5f,  0.5f,  0.5f}},
                // Right face (X+)
                {{ 0.5f,  0.5f,  0.5f}}, {{ 0.5f, -0.5f,  0.5f}}, {{ 0.5f, -0.5f, -0.5f}},
                {{ 0.5f, -0.5f, -0.5f}}, {{ 0.5f,  0.5f, -0.5f}}, {{ 0.5f,  0.5f,  0.5f}},
                // Top face (Y+)
                {{-0.5f,  0.5f, -0.5f}}, {{-0.5f,  0.5f,  0.5f}}, {{ 0.5f,  0.5f,  0.5f}},
                {{ 0.5f,  0.5f,  0.5f}}, {{ 0.5f,  0.5f, -0.5f}}, {{-0.5f,  0.5f, -0.5f}},
                // Bottom face (Y-)
                {{-0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f,  0.5f}},
                {{ 0.5f, -0.5f,  0.5f}}, {{-0.5f, -0.5f,  0.5f}}, {{-0.5f, -0.5f, -0.5f}}
            };
            return mesh;
        }
    };
}
