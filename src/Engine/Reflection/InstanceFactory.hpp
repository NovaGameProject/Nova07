// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <memory>
#include <string>
#include <functional>
#include <map>
#include "Engine/Nova.hpp"

namespace Nova {
    class InstanceFactory {
    public:
        // Use the full namespace to avoid ambiguity
        using Creator = std::function<std::shared_ptr<Nova::Instance>()>;

        // Rename this to Get() to avoid shadowing the Instance class name
        static InstanceFactory& Get() {
            static InstanceFactory factory;
            return factory;
        }

        template<typename T>
        void Register(const std::string& className) {
            creators[className] = []() { return std::make_shared<T>(); };
        }

        std::shared_ptr<Nova::Instance> Create(const std::string& className) {
            if (creators.contains(className)) {
                return creators[className]();
            }
            return nullptr;
        }
    private:
        std::map<std::string, Creator> creators;

        InstanceFactory() {
            // all our classes here!

            Register<Workspace>("Workspace");
            Register<Lighting>("Lighting");
            Register<Camera>("Camera");

            // Physical Objects
            Register<Part>("Part");
            Register<Seat>("Seat");
            Register<SpawnLocation>("SpawnLocation");

            // Components
            Register<SpecialMesh>("SpecialMesh");
            // Register<Decal>("Decal"); TODO: implement later, when we add ContentId (for loading stuff from site) and Base64: consider converting to ContentId when
            // SaaS (hosted Nova as platform)
            Register<Sky>("Sky");

            // Containers
            Register<Model>("Model");
        }
    };
}
