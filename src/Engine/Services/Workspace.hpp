#pragma once
#include "Engine/Nova.hpp"
#include <memory>

namespace Nova {

    namespace Props {
        struct WorkspaceProps {
            rfl::Flatten<InstanceProps> base;
        };
    }

    class Workspace : public Instance {
    public:

        float Gravity = 196.2f;

        // reflect-cpp unsupported:
        std::weak_ptr<Camera> CurrentCamera;
        std::weak_ptr<Instance> PrimaryPart;

        Props::WorkspaceProps props;
        NOVA_OBJECT(Workspace, props)

        Workspace() : Instance("Workspace") {}
    };

}
