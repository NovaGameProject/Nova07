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
        Seat() : Part("Seat") {}

        NOVA_OBJECT(Seat, props)
    };

}
