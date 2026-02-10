// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    class DataModel : public Instance {
    public:
        DataModel() : Instance("Game") {}
        NOVA_OBJECT_NO_PROPS(DataModel)

        template<typename T>
        std::shared_ptr<T> GetService() {
            for (auto& child : children) {
                if (auto service = std::dynamic_pointer_cast<T>(child)) return service;
            }
            auto s = std::make_shared<T>();
            s->SetParent(shared_from_this());
            return s;
        }
    };
}
