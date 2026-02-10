#pragma once
#include "Common/MathTypes.hpp"
#include "Engine/Objects/Instance.hpp"
#include <rfl/Flatten.hpp>

namespace Nova {
    namespace Props {
        struct BasePartProps {
            rfl::Flatten<Props::InstanceProps> base;
            rfl::Rename<"CFrame", CFrameReflect> CFrame;

            Vector3Reflect size = {4.0f, 1.2f, 2.0f};
            bool Anchored = false;
            bool CanCollide = true;
            float Transparency = 0.0f;
            int BrickColor = 194; // Medium Stone Grey
        };
    }

    class BasePart : public Instance {
    public:
        Props::BasePartProps props;
        BasePart(std::string name) : Instance(name) {}

        NOVA_OBJECT(BasePart, props)
    };
}
