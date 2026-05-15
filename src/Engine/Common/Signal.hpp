#pragma once
#include <lua.h>
#include <lualib.h>

// Avoid Luau macro / LuaBridge helper collision
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <LuaBridge/LuaBridge.h>

#include <vector>
#include <memory>
#include <functional>

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

        template<typename... Args>
        void fire(Args&&... args) {
            for (auto it = connections.begin(); it != connections.end();) {
                if (!(*it)->connected) {
                    it = connections.erase(it);
                } else {
                    try {
                        (*it)->callback(std::forward<Args>(args)...);
                    } catch (const luabridge::LuaException& e) {
                        // Silently ignore signal errors to avoid spam
                    }
                    ++it;
                }
            }
        }

    private:
        std::vector<std::shared_ptr<LuaConnection>> connections;
    };
}
