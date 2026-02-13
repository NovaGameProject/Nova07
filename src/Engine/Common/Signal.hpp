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

// Avoid Luau macro / LuaBridge helper collision
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <luabridge3/LuaBridge/LuaBridge.h>

#include <vector>
#include <memory>
#include <functional>
#include <iostream>

namespace Nova {
    class Connection {
    public:
        Connection(std::function<void()> disconnectFunc) : disconnectFunc(disconnectFunc) {}
        void disconnect() { if (disconnectFunc) disconnectFunc(); }
    private:
        std::function<void()> disconnectFunc;
    };

    class Signal {
    public:
        struct LuaConnection : public std::enable_shared_from_this<LuaConnection> {
            luabridge::LuaRef callback;
            bool connected = true;

            LuaConnection(luabridge::LuaRef cb) : callback(cb) {}
            void Disconnect() { connected = false; }
        };

        std::shared_ptr<LuaConnection> connect(luabridge::LuaRef callback) {
            auto conn = std::make_shared<LuaConnection>(callback);
            connections.push_back(conn);
            return conn;
        }

        // For C++ side firing with variadic args
        template<typename... Args>
        void fire(Args&&... args) {
            for (auto it = connections.begin(); it != connections.end();) {
                if (!(*it)->connected) {
                    it = connections.erase(it);
                } else {
                    try {
                        (*it)->callback(std::forward<Args>(args)...);
                    } catch (const luabridge::LuaException& e) {
                        std::cerr << "Signal Error: " << e.what() << std::endl;
                    }
                    ++it;
                }
            }
        }

    private:
        std::vector<std::shared_ptr<LuaConnection>> connections;
    };
}
