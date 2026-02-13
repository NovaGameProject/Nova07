// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <rfl/Generic.hpp>
#include <lua.h>
#include <lualib.h>

// Avoid Luau macro / LuaBridge helper collision
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <luabridge3/LuaBridge/LuaBridge.h>

#include "Common/MathTypes.hpp"

namespace Nova {
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    inline luabridge::LuaRef genericToLua(lua_State* L, const rfl::Generic& g) {
        return std::visit(overloaded{
            [&](double v) { return luabridge::LuaRef(L, v); },
            [&](int64_t v) { return luabridge::LuaRef(L, (double)v); },
            [&](bool v) { return luabridge::LuaRef(L, v); },
            [&](const std::string& v) { return luabridge::LuaRef(L, v); },
            [&](const rfl::Object<rfl::Generic>& v) {
                // If it's an object, it might be a Vector3 or Color3 if it has x,y,z or r,g,b
                if (v.size() == 3) {
                    if (v.get("x") && v.get("y") && v.get("z")) {
                        Vector3 vec(
                            v.get("x")->to_double().value_or(0.0),
                            v.get("y")->to_double().value_or(0.0),
                            v.get("z")->to_double().value_or(0.0)
                        );
                        return luabridge::LuaRef(L, vec);
                    }
                    if (v.get("r") && v.get("g") && v.get("b")) {
                        Color3 col(
                            v.get("r")->to_double().value_or(0.0),
                            v.get("g")->to_double().value_or(0.0),
                            v.get("b")->to_double().value_or(0.0)
                        );
                        return luabridge::LuaRef(L, col);
                    }
                }
                // Fallback to table
                auto table = luabridge::newTable(L);
                for (const auto& kv : v) {
                    table[kv.first] = genericToLua(L, kv.second);
                }
                return table;
            },
            [&](const std::vector<rfl::Generic>& v) {
                auto table = luabridge::newTable(L);
                for (size_t i = 0; i < v.size(); ++i) {
                    table[i + 1] = genericToLua(L, v[i]);
                }
                return table;
            },
            [&](std::nullopt_t) { return luabridge::LuaRef(L); },
            [&](auto&&) { return luabridge::LuaRef(L); }
        }, g.variant());
    }

    inline rfl::Generic luaToGeneric(luabridge::LuaRef v) {
        if (v.isNumber()) return rfl::Generic(v.unsafe_cast<double>());
        if (v.isString()) return rfl::Generic(v.unsafe_cast<std::string>());
        if (v.isBool()) return rfl::Generic(v.unsafe_cast<bool>());
        if (v.isUserdata()) {
            // Try to cast to common types
            auto L = v.state();
            luabridge::push(L, v);
            if (luabridge::Stack<Vector3>::isInstance(L, -1)) {
                auto res = v.cast<Vector3>();
                lua_pop(L, 1);
                if (res) {
                    auto vec = res.value();
                    rfl::Generic::Object obj;
                    obj.insert(std::string("x"), rfl::Generic((double)vec.x));
                    obj.insert(std::string("y"), rfl::Generic((double)vec.y));
                    obj.insert(std::string("z"), rfl::Generic((double)vec.z));
                    return rfl::Generic(obj);
                }
            } else {
                lua_pop(L, 1);
            }
        }
        return rfl::Generic();
    }
}
