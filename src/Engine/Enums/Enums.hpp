#pragma once

#include <rfl.hpp>

namespace Nova {
    enum class PartType {
        Ball = 0,
        Block,
        Sphere
    };

    enum class SurfaceType {
        Smooth = 0,
        Glue = 1,
        Weld = 2,
        Studs = 3,
        Inlets = 4,
        Universal = 5,
        Hinge = 6,
        Motor = 7,
        SteppingMotor = 8
    };

    enum class CameraType {
        Fixed = 0,
        Attach,
        Watch,
        Track,
        Follow,
        Custom
    };
}
