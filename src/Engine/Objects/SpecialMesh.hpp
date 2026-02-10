#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Objects/Instance.hpp"
#include <rfl/Flatten.hpp>
#include <string>

namespace Nova {
    namespace Props {
        struct MeshProps {
            rfl::Flatten<Props::InstanceProps> base;
            std::string MeshId = "";
            std::string TextureId = "";
            Vector3Reflect Scale = {1, 1, 1};
        };
    }

    class SpecialMesh : public Instance {
    public:
        Props::MeshProps props;
        SpecialMesh() : Instance("SpecialMesh") {}

        NOVA_OBJECT(SpecialMesh, props)
    };
}
