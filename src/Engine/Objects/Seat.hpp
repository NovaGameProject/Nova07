// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <rfl/Flatten.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/Part.hpp"

namespace Nova {

    namespace Props {
        struct SeatPartProps {
            rfl::Flatten<PartProps> base;
        };
    }


    class Seat : public Part {
    public:
        Props::SeatPartProps props;
        Seat() : Part("Seat") {
            basePartProps = &props.base.get().base.get();
        }

        NOVA_OBJECT(Seat, props)
    };

}
