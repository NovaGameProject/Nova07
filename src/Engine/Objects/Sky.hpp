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
