// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <lua.h>
#include <lualib.h>

#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <LuaBridge/LuaBridge.h>

#include "Common/PropertyValue.hpp"

namespace Nova {
    inline luabridge::LuaRef propertyValueToLua(lua_State* L, const PropertyValue& v) {
        switch (v.kind) {
            case PropertyValue::Kind::Nil:
                return luabridge::LuaRef(L);
            case PropertyValue::Kind::Bool:
                return luabridge::LuaRef(L, v.toBool());
            case PropertyValue::Kind::Int:
                return luabridge::LuaRef(L, static_cast<double>(v.toInt()));
            case PropertyValue::Kind::Float:
                return luabridge::LuaRef(L, v.toFloat());
            case PropertyValue::Kind::String:
                return luabridge::LuaRef(L, v.toString());
            case PropertyValue::Kind::Vector3:
                return luabridge::LuaRef(L, v.toVector3());
            case PropertyValue::Kind::CFrame:
                // CFrame isn't directly pushable via LuaBridge yet,
                // so we store it as a special userdata or just return nil for now
                return luabridge::LuaRef(L);
            case PropertyValue::Kind::Color3:
                return luabridge::LuaRef(L, v.toColor3());
        }
        return luabridge::LuaRef(L);
    }

    inline PropertyValue luaToPropertyValue(luabridge::LuaRef v) {
        if (v.isNil()) return PropertyValue();
        if (v.isBool()) return PropertyValue(v.unsafe_cast<bool>());
        if (v.isNumber()) return PropertyValue(v.unsafe_cast<double>());
        if (v.isString()) return PropertyValue(v.unsafe_cast<std::string>());
        if (v.isUserdata()) {
            auto L = v.state();
            luabridge::push(L, v);
            if (luabridge::Stack<Vector3>::isInstance(L, -1)) {
                auto res = v.cast<Vector3>();
                lua_pop(L, 1);
                if (res) return PropertyValue(res.value());
            } else if (luabridge::Stack<Color3>::isInstance(L, -1)) {
                auto res = v.cast<Color3>();
                lua_pop(L, 1);
                if (res) return PropertyValue(res.value());
            } else {
                lua_pop(L, 1);
            }
        }
        return PropertyValue();
    }
}
