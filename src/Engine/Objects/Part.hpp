// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <rfl/Flatten.hpp>
#include <rfl/Field.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Common/BrickColors.hpp"
namespace Nova {

    namespace Props {
        struct PartProps {
            rfl::Flatten<BasePartProps> base;

            rfl::Rename<"shape", PartType> shape = PartType::Block;
        };
    }


    class Part : public BasePart {
    public:
        Props::PartProps props;
        // when Part is used as a Base for SpawnLocation or Seat
        Part(std::string name) : BasePart(name) {
            basePartProps = &props.base.get();
        }

        // standalone Part
        Part() : BasePart("Part") {
            basePartProps = &props.base.get();
        }

        NOVA_OBJECT(Part, props)
    };

}
