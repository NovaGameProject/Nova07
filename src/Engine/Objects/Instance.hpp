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


namespace Nova {
    namespace Props {
        struct InstanceProps {
            rfl::Rename<"Name", std::string> Name;
            rfl::Rename<"archivable", bool> Archivable = true;
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
            auto result = rfl::from_generic<decltype(this->PropsMember), rfl::UnderlyingEnums>(generic); \
            if (result) { \
                this->PropsMember = result.value(); \
            } else { \
                std::cout << "[REFLECTION ERROR] " << #ClassName << " failed: " \
                          << result.error().what() << std::endl; \
            } \
        } \
        std::string GetName() const override { \
            /* Now we get the whole struct and just pick the Name out of it */ \
            auto& instance = ::Nova::Internal::get_instance_props(this->PropsMember); \
            return std::string(instance.Name.value()); \
        }

    // For classes WITHOUT props (like Folder/DataModel)
    #define NOVA_OBJECT_NO_PROPS(ClassName) \
        std::string GetClassName() const override { return #ClassName; } \
        void ApplyPropertiesGeneric(const rfl::Generic& generic) override {} \
        rfl::Generic GetPropertiesGeneric() const override { return rfl::Generic(rfl::Object<rfl::Generic>()); } \
        std::string GetName() const override { return m_debugName; }

    class Instance : public std::enable_shared_from_this<Instance> {
    public:
        virtual ~Instance() = default;

        virtual std::string GetClassName() const = 0;
        virtual std::string GetName() const = 0;

        virtual void ApplyPropertiesGeneric(const rfl::Generic& generic) = 0;
        virtual rfl::Generic GetPropertiesGeneric() const = 0;

        std::weak_ptr<Instance> parent;
        std::vector<std::shared_ptr<Instance>> children;

        std::string m_debugName;
        Instance(std::string name) : m_debugName(name) {}

        std::shared_ptr<Instance> GetParent() const { return parent.lock(); }
        std::vector<std::shared_ptr<Instance>> GetChildren() const { return children; }

        void SetParent(std::shared_ptr<Instance> newParent) {
            auto self = shared_from_this();
            if (auto p = parent.lock()) {
                auto& c = p->children;
                c.erase(std::remove(c.begin(), c.end(), self), c.end());
            }
            parent = newParent;
            if (newParent) newParent->children.push_back(self);
        }

        bool IsRoot() const { return parent.expired(); }
    };
}
