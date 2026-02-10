// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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
