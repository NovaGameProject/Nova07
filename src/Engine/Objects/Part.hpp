// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/BasePart.hpp"

namespace Nova {
    class Part : public BasePart {
    public:
        PartType shape = PartType::Block;

        Part(std::string name) : BasePart(name) {}
        Part() : BasePart("Part") {}

        std::string GetClassName() const override { return "Part"; }
    };
}
