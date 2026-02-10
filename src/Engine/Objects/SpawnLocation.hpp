// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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

        glm::mat4 GetLocalTransform() override {
            return props.base.get().base.get().CFrame.get().to_nova().to_mat4();
        }

        glm::vec3 GetSize() override {
            return props.base.get().base.get().size.to_glm();
        }

        NOVA_OBJECT(SpawnLocation, props)
    };

}
