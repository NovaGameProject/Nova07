#pragma once
#include "Engine/Objects/Part.hpp"
#include <rfl/Flatten.hpp>

namespace Nova {

    namespace Props {
        struct SpawnLocationProps {
            rfl::Flatten<PartProps> base;
        };
    }


    class SpawnLocation : public Part {
    public:
        Props::SpawnLocationProps props;
        SpawnLocation() : Part("SpawnLocation") {}

        NOVA_OBJECT(SpawnLocation, props)
    };

}
