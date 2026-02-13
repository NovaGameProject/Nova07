// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Reflection/ClassDescriptor.hpp"

namespace Nova {
    std::map<std::string, std::shared_ptr<ClassDescriptor>>& ClassDescriptor::GetAll() {
        static std::map<std::string, std::shared_ptr<ClassDescriptor>> all;
        return all;
    }

    std::shared_ptr<ClassDescriptor> ClassDescriptor::Get(const std::string& name) {
        auto& all = GetAll();
        if (all.contains(name)) return all[name];
        return nullptr;
    }
}
