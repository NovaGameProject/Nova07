#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Objects/Instance.hpp"

namespace Nova {

    namespace Props {
        struct LightingProps {
            rfl::Flatten<InstanceProps> base;
            Color3Reflect TopAmbientV9;
            Color3Reflect BottomAmbientV9;
            Color3Reflect SpotLightV9;
            Color3Reflect ClearColor;
            float GeographicLatitude;
            std::string TimeOfDay;
        };
    }

    class Lighting : public Instance {
    public:
        Props::LightingProps props;
        NOVA_OBJECT(Lighting, props)
        Lighting() : Instance("Lighting") {}
    };
}
