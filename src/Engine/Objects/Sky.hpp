#pragma once
#include <rfl/Flatten.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    namespace Props {
        struct SkyProps {
            rfl::Flatten<InstanceProps> base;
            std::string SkyboxBk = "rbxasset://textures/sky/sky_bk.png";
            std::string SkyboxDn = "rbxasset://textures/sky/sky_dn.png";
            std::string SkyboxFt = "rbxasset://textures/sky/sky_ft.png";
            std::string SkyboxLf = "rbxasset://textures/sky/sky_lf.png";
            std::string SkyboxRt = "rbxasset://textures/sky/sky_rt.png";
            std::string SkyboxUp = "rbxasset://textures/sky/sky_up.png";
            int StarCount = 3000;
        };
    }

    class Sky : public Instance {
    public:
        Props::SkyProps props;
        Sky() : Instance("Sky") {}

        NOVA_OBJECT(Sky, props)
    };
}
