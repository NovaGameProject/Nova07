// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Objects/Instance.hpp"

namespace Nova {

    namespace Props {
        struct LightingProps {
            rfl::Flatten<InstanceProps> base;
            Color3Reflect TopAmbientV9 = {0.5f, 0.5f, 0.5f};
            Color3Reflect BottomAmbientV9 = {0.2f, 0.2f, 0.2f};
            Color3Reflect SpotLightV9 = {1.0f, 1.0f, 1.0f};
            Color3Reflect ClearColor = {0.517647f, 0.694118f, 0.972549f};
            float GeographicLatitude = 41.7333f;
            std::string TimeOfDay = "14:00:00";
        };
    }

    class Lighting : public Instance {
    public:
        Props::LightingProps props;
        NOVA_OBJECT(Lighting, props)
        Lighting() : Instance("Lighting") {}
    };
}
