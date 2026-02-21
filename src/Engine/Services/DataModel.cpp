// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/DataModel.hpp"
#include "Engine/Objects/InstanceFactory.hpp"

namespace Nova {
    std::shared_ptr<Instance> DataModel::GetService(const std::string& className) {
        for (auto& child : children) {
            if (child->GetClassName() == className) return child;
        }

        auto s = InstanceFactory::Get().Create(className);
        if (s) {
            s->SetParent(shared_from_this());
        }
        return s;
    }
}
