#pragma once
#include <rfl/Flatten.hpp>
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    namespace Props {
        struct ModelProps {
            rfl::Flatten<InstanceProps> base;
        };
    }

    class Model : public Instance {
    public:
        Props::ModelProps props;
        Model() : Instance("Model") {}

        // unsupported by reflection
        // reference to an instance, handled using referents in ROBLOX
        std::weak_ptr<Instance> PrimaryPart;

        NOVA_OBJECT(Model, props)
    };
}
