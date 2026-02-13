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

#include <rfl.hpp>
#include <rfl/Generic.hpp>
#include <rfl/to_generic.hpp>
#include <rfl/Object.hpp>

#include <lua.h>
#include <lualib.h>

// Avoid Luau macro / LuaBridge helper collision
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <luabridge3/LuaBridge/LuaBridge.h>

namespace Nova {
    class DataModel;

    namespace Props {
        struct InstanceProps {
            std::string Name;
            bool Archivable = true;
        };
    }

    namespace Internal {
        // Finds the InstanceProps struct regardless of nesting depth
        template <typename T>
        auto& get_instance_props(T& props) {
            if constexpr (std::is_same_v<T, ::Nova::Props::InstanceProps>) {
                return props;
            } else {
                // Recurse into the flattened base
                return get_instance_props(props.base.get());
            }
        }

        // Const version for GetName()
        template <typename T>
        auto& get_instance_props(const T& props) {
            if constexpr (std::is_same_v<T, ::Nova::Props::InstanceProps>) {
                return props;
            } else {
                return get_instance_props(props.base.get());
            }
        }
    }

    // For classes WITH props (like Part)
    #define NOVA_OBJECT(ClassName, PropsMember) \
        std::string GetClassName() const override { return #ClassName; } \
        \
        rfl::Generic GetPropertiesGeneric() const override { \
            return rfl::to_generic<rfl::UnderlyingEnums>(this->PropsMember); \
        } \
        \
        void ApplyPropertiesGeneric(const rfl::Generic& generic) override { \
            auto current = rfl::to_generic<rfl::UnderlyingEnums>(this->PropsMember); \
            if (auto* dest = std::get_if<rfl::Object<rfl::Generic>>(&current.variant())) { \
                if (auto* src = std::get_if<rfl::Object<rfl::Generic>>(&generic.variant())) { \
                    for (const auto& entry : *src) { \
                        const auto& name = entry.first; \
                        const auto& value = entry.second; \
                        /* Update in-place to avoid duplicates */ \
                        auto it = std::find_if(dest->begin(), dest->end(), [&](const auto& p) { return p.first == name; }); \
                        if (it != dest->end()) { \
                            it->second = value; \
                        } \
                    } \
                    auto result = rfl::from_generic<decltype(this->PropsMember), rfl::UnderlyingEnums>(current); \
                    if (result) { \
                        this->PropsMember = result.value(); \
                    } else { \
                        std::cout << "[REFLECTION ERROR] " << #ClassName << " failed: " \
                                  << result.error().what() << std::endl; \
                    } \
                } \
            } \
        } \
        \
        rfl::Generic GetProperty(const std::string& name) const override { \
            auto generic = rfl::to_generic<rfl::UnderlyingEnums>(this->PropsMember); \
            if (auto* obj = std::get_if<rfl::Object<rfl::Generic>>(&generic.variant())) { \
                if (auto val = obj->get(name)) return *val; \
            } \
            return rfl::Generic(); \
        } \
        \
        bool SetProperty(const std::string& name, const rfl::Generic& value) override { \
            auto generic = rfl::to_generic<rfl::UnderlyingEnums>(this->PropsMember); \
            if (auto* obj = std::get_if<rfl::Object<rfl::Generic>>(&generic.variant())) { \
                auto it = std::find_if(obj->begin(), obj->end(), [&](const auto& p) { return p.first == name; }); \
                if (it != obj->end()) { \
                    it->second = value; \
                    auto result = rfl::from_generic<decltype(this->PropsMember), rfl::UnderlyingEnums>(generic); \
                    if (result) { \
                        this->PropsMember = result.value(); \
                        OnPropertyChanged(name); \
                        return true; \
                    } \
                } \
            } \
            return false; \
        } \
        \
        std::string GetName() const override { \
            /* Now we get the whole struct and just pick the Name out of it */ \
            auto& instance = ::Nova::Internal::get_instance_props(this->PropsMember); \
            return std::string(instance.Name); \
        }

    // For classes WITHOUT props (like Folder/DataModel)
    #define NOVA_OBJECT_NO_PROPS(ClassName) \
        std::string GetClassName() const override { return #ClassName; } \
        void ApplyPropertiesGeneric(const rfl::Generic& generic) override {} \
        rfl::Generic GetPropertiesGeneric() const override { return rfl::Generic(rfl::Object<rfl::Generic>()); } \
        rfl::Generic GetProperty(const std::string& name) const override { return rfl::Generic(); } \
        bool SetProperty(const std::string& name, const rfl::Generic& value) override { return false; } \
        std::string GetName() const override { return m_debugName; }

    class Instance : public std::enable_shared_from_this<Instance> {
    public:
        virtual ~Instance() = default;

        virtual std::string GetClassName() const = 0;
        virtual std::string GetName() const = 0;

        virtual void ApplyPropertiesGeneric(const rfl::Generic& generic) = 0;
        virtual rfl::Generic GetPropertiesGeneric() const = 0;

        // Automatic Property System
        virtual rfl::Generic GetProperty(const std::string& name) const = 0;
        virtual bool SetProperty(const std::string& name, const rfl::Generic& value) = 0;

        virtual void OnPropertyChanged(const std::string& name) {}

        std::weak_ptr<Instance> parent;
        std::vector<std::shared_ptr<Instance>> children;

        std::string m_debugName;
        Instance(std::string name) : m_debugName(name) {}

        std::shared_ptr<Instance> GetParent() const { return parent.lock(); }
        const std::vector<std::shared_ptr<Instance>>& GetChildren() const { return children; }

        void SetParent(std::shared_ptr<Instance> newParent);

        std::shared_ptr<DataModel> GetDataModel();
        bool IsDescendantOf(std::shared_ptr<Instance> other);

        virtual void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent);

        // Lua Meta Methods for dynamic properties (Static version for reliable binding)
        static luabridge::LuaRef LuaIndex(Instance& self, const luabridge::LuaRef& key, lua_State* L);
        static luabridge::LuaRef LuaNewIndex(Instance& self, const luabridge::LuaRef& key, const luabridge::LuaRef& value, lua_State* L);

        bool IsRoot() const { return parent.expired(); }
    };
}
