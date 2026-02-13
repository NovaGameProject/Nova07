// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "Engine/Common/Signal.hpp"

namespace Nova {
    class Instance;

    struct MethodDescriptor {
        std::string name;
        std::function<int(lua_State*)> handler;
    };

    struct SignalDescriptor {
        std::string name;
        std::function<Signal*(Instance*)> getter;
    };

    class ClassDescriptor {
    public:
        std::string className;
        std::string baseClassName;
        std::map<std::string, MethodDescriptor> methods;
        std::map<std::string, SignalDescriptor> signals;

        static std::map<std::string, std::shared_ptr<ClassDescriptor>>& GetAll();
        static std::shared_ptr<ClassDescriptor> Get(const std::string& name);
    };

    void RegisterClasses();

    template<typename T>
    class ClassDescriptorBuilder {
        std::shared_ptr<ClassDescriptor> desc;
    public:
        ClassDescriptorBuilder(const std::string& name, const std::string& base = "") {
            desc = std::make_shared<ClassDescriptor>();
            desc->className = name;
            desc->baseClassName = base;
            ClassDescriptor::GetAll()[name] = desc;
        }

        template<typename Func>
        ClassDescriptorBuilder& Method(const std::string& name, Func func) {
            desc->methods[name] = {name, [func](lua_State* L) {
                // This is a simplified handler, luabridge usually does better
                // But for now let's just use it as a placeholder or integrate with luabridge
                return 0; 
            }};
            return *this;
        }

        template<typename SignalPtr>
        ClassDescriptorBuilder& Signal(const std::string& name, SignalPtr ptr) {
            desc->signals[name] = {name, [ptr](Instance* inst) {
                T* obj = static_cast<T*>(inst);
                return &(obj->*ptr);
            }};
            return *this;
        }
    };
}
