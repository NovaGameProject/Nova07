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
#include <memory>
#include <algorithm>
#include <iostream>

#include <lua.h>
#include <lualib.h>

#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <LuaBridge/LuaBridge.h>

namespace Nova {
    class DataModel;

    using NetworkID = uint32_t;

    class Instance : public std::enable_shared_from_this<Instance> {
    public:
        virtual ~Instance() = default;

        virtual std::string GetClassName() const = 0;
        virtual std::string GetName() const = 0;

        virtual void OnPropertyChanged(const std::string& name) {}

        std::weak_ptr<Instance> parent;
        std::vector<std::shared_ptr<Instance>> children;

        std::string m_debugName;
        NetworkID networkID = 0;  // 0 = not replicated
        Instance(std::string name) : m_debugName(name) {}

        std::shared_ptr<Instance> GetParent() const { return parent.lock(); }
        const std::vector<std::shared_ptr<Instance>>& GetChildren() const { return children; }

        std::shared_ptr<Instance> FindFirstChild(const std::string& name, bool recursive = false);
        std::shared_ptr<Instance> FindFirstChildOfClass(const std::string& className);
        std::shared_ptr<Instance> FindFirstChildWhichIsA(const std::string& className, bool recursive = false);
        std::shared_ptr<Instance> FindFirstAncestor(const std::string& name);
        std::shared_ptr<Instance> FindFirstAncestorOfClass(const std::string& className);
        std::shared_ptr<Instance> FindFirstAncestorWhichIsA(const std::string& className);

        std::vector<std::shared_ptr<Instance>> GetDescendants();
        std::string GetFullName();

        bool IsA(const std::string& className);
        std::shared_ptr<Instance> Clone();
        void Destroy();

        void SetParent(std::shared_ptr<Instance> newParent);

        std::shared_ptr<DataModel> GetDataModel();
        bool IsDescendantOf(std::shared_ptr<Instance> other);

        virtual void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent);

        static luabridge::LuaRef LuaIndex(Instance& self, const luabridge::LuaRef& key, lua_State* L);
        static luabridge::LuaRef LuaNewIndex(Instance& self, const luabridge::LuaRef& key, const luabridge::LuaRef& value, lua_State* L);

        bool IsRoot() const { return parent.expired(); }
        bool IsDestroyed() const { return m_destroyed; }

    protected:
        bool m_destroyed = false;
    };
}
