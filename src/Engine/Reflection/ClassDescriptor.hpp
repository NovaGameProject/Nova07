// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <iostream>
#include "Common/PropertyValue.hpp"
#include "Engine/Common/Signal.hpp"

#include <lua.h>
#include <lualib.h>
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <LuaBridge/LuaBridge.h>

namespace Nova {
    class Instance;

    // Property kind enum for type-erased access
    enum class PropertyKind {
        Bool, Int, Float, String,
        Vector3, CFrame, Color3
    };

    // Type-erased property accessor interface
    struct IPropertyAccessor {
        virtual ~IPropertyAccessor() = default;
        virtual PropertyKind kind() const = 0;
        virtual PropertyValue get(const Instance* inst) const = 0;
        virtual bool set(Instance* inst, const PropertyValue& value) const = 0;
    };

    // Typed property accessor using member pointers
    template<typename T, typename U>
    class PropertyAccessor : public IPropertyAccessor {
        U T::* member;
        PropertyKind kind_;
    public:
        PropertyAccessor(U T::* m, PropertyKind k) : member(m), kind_(k) {}

        PropertyKind kind() const override { return kind_; }

        PropertyValue get(const Instance* inst) const override {
            auto* obj = static_cast<const T*>(inst);
            return toPropertyValue(obj->*member);
        }

        bool set(Instance* inst, const PropertyValue& value) const override {
            auto* obj = static_cast<T*>(inst);
            auto result = fromPropertyValue<U>(value);
            if (result) {
                obj->*member = *result;
                return true;
            }
            return false;
        }

    private:
        template<typename V>
        static PropertyValue toPropertyValue(const V& v) {
            if constexpr (std::is_same_v<V, bool>) return PropertyValue(v);
            else if constexpr (std::is_integral_v<V>) return PropertyValue(static_cast<int64_t>(v));
            else if constexpr (std::is_floating_point_v<V>) return PropertyValue(static_cast<double>(v));
            else if constexpr (std::is_same_v<V, std::string>) return PropertyValue(v);
            else if constexpr (std::is_same_v<V, Vector3>) return PropertyValue(v);
            else if constexpr (std::is_same_v<V, CFrame>) return PropertyValue(v);
            else if constexpr (std::is_same_v<V, Color3>) return PropertyValue::FromColor3(v);
            else if constexpr (std::is_enum_v<V>) return PropertyValue(static_cast<int64_t>(v));
            else return PropertyValue();
        }

        template<typename V>
        static std::optional<V> fromPropertyValue(const PropertyValue& v) {
            if constexpr (std::is_same_v<V, bool>) {
                if (v.isBool()) return v.toBool();
                if (v.isInt()) return v.toInt() != 0;
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, int64_t>) {
                if (v.isInt()) return v.toInt();
                if (v.isFloat()) return static_cast<int64_t>(v.toFloat());
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, int>) {
                if (v.isInt()) return static_cast<int>(v.toInt());
                if (v.isFloat()) return static_cast<int>(v.toFloat());
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, float>) {
                if (v.isFloat()) return static_cast<float>(v.toFloat());
                if (v.isInt()) return static_cast<float>(v.toInt());
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, double>) {
                if (v.isFloat()) return v.toFloat();
                if (v.isInt()) return static_cast<double>(v.toInt());
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, std::string>) {
                if (v.isString()) return v.toString();
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, Vector3>) {
                if (v.isVector3()) return v.toVector3();
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, CFrame>) {
                if (v.isCFrame()) return v.toCFrame();
                return std::nullopt;
            }
            else if constexpr (std::is_same_v<V, Color3>) {
                if (v.isColor3()) return v.toColor3();
                return std::nullopt;
            }
            else if constexpr (std::is_enum_v<V>) {
                if (v.isInt()) return static_cast<V>(v.toInt());
                return std::nullopt;
            }
            return std::nullopt;
        }
    };

    // Method descriptor — a Lua-callable function bound to an Instance
    struct MethodDescriptor {
        std::string name;
        std::function<int(lua_State*, Instance*)> call;
    };

    // Signal descriptor — extracts a Signal* from an Instance
    struct SignalDescriptor {
        std::string name;
        std::function<Signal*(Instance*)> getter;
    };

    // Runtime class metadata
    class ClassDescriptor {
    public:
        std::string className;
        std::string baseClassName;
        ClassDescriptor* baseClass = nullptr; // Resolved after registration

        std::map<std::string, std::shared_ptr<IPropertyAccessor>> properties;
        std::map<std::string, MethodDescriptor> methods;
        std::map<std::string, SignalDescriptor> signals;

        // Walk inheritance chain to find a property
        const IPropertyAccessor* FindProperty(const std::string& name) const {
            const ClassDescriptor* current = this;
            while (current) {
                auto it = current->properties.find(name);
                if (it != current->properties.end()) return it->second.get();
                current = current->baseClass;
            }
            return nullptr;
        }

        // Walk inheritance chain to find a method
        const MethodDescriptor* FindMethod(const std::string& name) const {
            const ClassDescriptor* current = this;
            while (current) {
                auto it = current->methods.find(name);
                if (it != current->methods.end()) return &it->second;
                current = current->baseClass;
            }
            return nullptr;
        }

        // Walk inheritance chain to find a signal
        const SignalDescriptor* FindSignal(const std::string& name) const {
            const ClassDescriptor* current = this;
            while (current) {
                auto it = current->signals.find(name);
                if (it != current->signals.end()) return &it->second;
                current = current->baseClass;
            }
            return nullptr;
        }

        bool IsA(const std::string& className) const {
            const ClassDescriptor* current = this;
            while (current) {
                if (current->className == className) return true;
                current = current->baseClass;
            }
            return false;
        }

        static std::map<std::string, std::shared_ptr<ClassDescriptor>>& GetAll();
        static ClassDescriptor* Get(const std::string& name);
        static void ResolveInheritance();
    };

    void RegisterClasses();

    // Fluent builder for class registration
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

        template<typename U>
        ClassDescriptorBuilder& Property(const std::string& name, U T::* member) {
            PropertyKind k;
            using Raw = std::remove_cv_t<std::remove_reference_t<U>>;
            if constexpr (std::is_same_v<Raw, bool>) k = PropertyKind::Bool;
            else if constexpr (std::is_integral_v<Raw>) k = PropertyKind::Int;
            else if constexpr (std::is_floating_point_v<Raw>) k = PropertyKind::Float;
            else if constexpr (std::is_same_v<Raw, std::string>) k = PropertyKind::String;
            else if constexpr (std::is_same_v<Raw, Vector3>) k = PropertyKind::Vector3;
            else if constexpr (std::is_same_v<Raw, CFrame>) k = PropertyKind::CFrame;
            else if constexpr (std::is_same_v<Raw, Color3>) k = PropertyKind::Color3;
            else k = PropertyKind::String; // fallback

            desc->properties[name] = std::make_shared<PropertyAccessor<T, U>>(member, k);
            return *this;
        }

        // Method binding: void(T::*)()
        template<typename Ret, typename... Args>
        ClassDescriptorBuilder& Method(const std::string& name, Ret(T::*func)(Args...)) {
            desc->methods[name] = {name, [func](lua_State* L, Instance* inst) -> int {
                auto* obj = static_cast<T*>(inst);
                if constexpr (std::is_void_v<Ret>) {
                    // Need to pop args from stack and call
                    return callLuaMethod(L, obj, func);
                } else {
                    return callLuaMethod(L, obj, func);
                }
            }};
            return *this;
        }

        // Signal binding
        template<typename SignalPtr>
        ClassDescriptorBuilder& Signal(const std::string& name, SignalPtr ptr) {
            desc->signals[name] = {name, [ptr](Instance* inst) -> Nova::Signal* {
                T* obj = static_cast<T*>(inst);
                return &(obj->*ptr);
            }};
            return *this;
        }

    private:
        // Lua method call helpers
        template<typename Ret, typename... Args>
        static int callLuaMethod(lua_State* L, T* obj, Ret(T::*func)(Args...)) {
            if constexpr (sizeof...(Args) == 0 && std::is_void_v<Ret>) {
                (obj->*func)();
                return 0;
            }
            else if constexpr (sizeof...(Args) == 0 && std::is_same_v<Ret, float>) {
                lua_pushnumber(L, (obj->*func)());
                return 1;
            }
            else if constexpr (sizeof...(Args) == 0 && std::is_same_v<Ret, Vector3>) {
                luabridge::push(L, (obj->*func)());
                return 1;
            }
            else if constexpr (sizeof...(Args) == 1 && std::is_void_v<Ret>) {
                using Arg0 = std::tuple_element_t<0, std::tuple<Args...>>;
                using RawArg0 = std::remove_cv_t<std::remove_reference_t<Arg0>>;
                if constexpr (std::is_same_v<RawArg0, float>) {
                    float arg = static_cast<float>(lua_tonumber(L, 1));
                    (obj->*func)(arg);
                    return 0;
                }
                else if constexpr (std::is_same_v<RawArg0, Vector3> || std::is_same_v<RawArg0, const Vector3&>) {
                    // Use LuaBridge to get Vector3 from stack
                    if (luabridge::Stack<Vector3>::isInstance(L, 1)) {
                        auto result = luabridge::Stack<Vector3>::get(L, 1);
                        if (result) {
                            (obj->*func)(result.value());
                        }
                    }
                    return 0;
                }
                return 0;
            }
            return 0;
        }
    };
}
