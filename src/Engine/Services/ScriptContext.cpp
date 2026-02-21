// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/ScriptContext.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/Lighting.hpp"
#include "Engine/Common/Signal.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Objects/InstanceFactory.hpp"
#include "Common/MathTypes.hpp"
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include <iostream>
#include <chrono>
#include <queue>

namespace Nova {

    struct ScheduledTask {
        double wakeTime;
        luabridge::LuaRef coroutine;
        bool isDelay = false;
        luabridge::LuaRef callback;

        ScheduledTask(lua_State* L) : coroutine(L), callback(L), wakeTime(0), isDelay(false) {}
    };

    static std::queue<ScheduledTask> g_taskQueue;
    static auto g_startTime = std::chrono::steady_clock::now();

    static double GetGameTime() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - g_startTime).count();
    }

    static int lua_print(lua_State* L) {
        int nargs = lua_gettop(L);
        for (int i = 1; i <= nargs; i++) {
            if (i > 1) std::cout << "\t";
            const char* s = luaL_tolstring(L, i, NULL);
            if (s) std::cout << s;
            lua_pop(L, 1);
        }
        std::cout << std::endl;
        return 0;
    }

    static int lua_tick(lua_State* L) {
        auto now = std::chrono::steady_clock::now();
        auto epoch = now.time_since_epoch();
        double seconds = std::chrono::duration<double>(epoch).count();
        lua_pushnumber(L, seconds);
        return 1;
    }

    static int lua_time(lua_State* L) {
        lua_pushnumber(L, GetGameTime());
        return 1;
    }

    static int lua_wait(lua_State* L) {
        double seconds = luaL_optnumber(L, 1, 0.0);
        if (seconds < 0) seconds = 0;
        
        double wakeTime = GetGameTime() + seconds;
        
        // Push current thread onto stack and create LuaRef from it
        lua_pushthread(L);
        luabridge::LuaRef coro = luabridge::LuaRef::fromStack(L, -1);
        lua_pop(L, 1);
        
        ScheduledTask task(L);
        task.wakeTime = wakeTime;
        task.coroutine = coro;
        g_taskQueue.push(task);
        
        return lua_yield(L, 0);
    }

    static int lua_delay(lua_State* L) {
        double seconds = luaL_checknumber(L, 1);
        if (!lua_isfunction(L, 2)) {
            luaL_error(L, "delay() requires a function as second argument");
            return 0;
        }

        double wakeTime = GetGameTime() + seconds;

        luabridge::LuaRef callback(L, 2);

        ScheduledTask task(L);
        task.wakeTime = wakeTime;
        task.isDelay = true;
        task.callback = callback;
        g_taskQueue.push(task);

        return 0;
    }

    static int lua_spawn(lua_State* L) {
        if (!lua_isfunction(L, 1)) {
            luaL_error(L, "spawn() requires a function argument");
            return 0;
        }

        double wakeTime = GetGameTime();

        luabridge::LuaRef callback(L, 1);

        ScheduledTask task(L);
        task.wakeTime = wakeTime;
        task.isDelay = true;
        task.callback = callback;
        g_taskQueue.push(task);

        return 0;
    }

    ScriptContext::ScriptContext() : Instance("ScriptContext") {
        InitializeVM();
    }

    ScriptContext::~ScriptContext() {
        if (L) lua_close(L);
    }

    void ScriptContext::InitializeVM() {
        L = luaL_newstate();
        luaL_openlibs(L);

        lua_pushcfunction(L, lua_print, "print");
        lua_setglobal(L, "print");

        lua_pushcfunction(L, lua_tick, "tick");
        lua_setglobal(L, "tick");

        lua_pushcfunction(L, lua_time, "time");
        lua_setglobal(L, "time");

        lua_pushcfunction(L, lua_wait, "wait");
        lua_setglobal(L, "wait");

        lua_pushcfunction(L, lua_delay, "delay");
        lua_setglobal(L, "delay");

        lua_pushcfunction(L, lua_spawn, "spawn");
        lua_setglobal(L, "spawn");

        BindAPI();
    }

    void ScriptContext::BindAPI() {
        luabridge::getGlobalNamespace(L)
            .beginClass<Signal>("Signal")
                .addFunction("connect", &Signal::connect)
                .addFunction("Connect", &Signal::connect)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .beginClass<Signal::LuaConnection>("Connection")
                .addFunction("disconnect", &Signal::LuaConnection::Disconnect)
                .addFunction("Disconnect", &Signal::LuaConnection::Disconnect)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .beginClass<Vector3>("Vector3")
                .addConstructor<void(*)(float, float, float)>()
                .addStaticFunction("new", +[](float x, float y, float z) { return Vector3(x, y, z); })
                .addProperty("x", &Vector3::x)
                .addProperty("y", &Vector3::y)
                .addProperty("z", &Vector3::z)
                .addProperty("X", &Vector3::x)
                .addProperty("Y", &Vector3::y)
                .addProperty("Z", &Vector3::z)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .beginClass<CFrame>("CFrame")
                .addConstructor<void(*)(void)>()
                .addStaticFunction("new", +[]() { return CFrame(); })
                .addProperty("p", &CFrame::position)
                .addProperty("Position", &CFrame::position)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .beginClass<Instance>("Instance")
                .addStaticFunction("new", +[](std::string className, luabridge::LuaRef parent) -> std::shared_ptr<Instance> {
                    auto inst = InstanceFactory::Get().Create(className);
                    if (inst && !parent.isNil()) {
                        auto L = parent.state();
                        parent.push();
                        if (luabridge::Stack<Instance>::isInstance(L, -1)) {
                            auto res = parent.cast<std::shared_ptr<Instance>>();
                            if (res) {
                                inst->SetParent(res.value());

                                if (auto dm = inst->GetDataModel()) {
                                    if (auto ws = dm->GetService<Workspace>()) {
                                        if (inst->IsDescendantOf(ws)) ws->RefreshCachedParts();
                                    }
                                }
                            }
                        }
                        lua_pop(L, 1);
                    }
                    return inst;
                })
                .addFunction("GetChildren", &Instance::GetChildren)
                .addFunction("GetParent", &Instance::GetParent)
                .addFunction("FindFirstChild", &Instance::FindFirstChild)
                .addFunction("findFirstChild", &Instance::FindFirstChild)
                .addFunction("FindFirstChildOfClass", &Instance::FindFirstChildOfClass)
                .addFunction("FindFirstChildWhichIsA", &Instance::FindFirstChildWhichIsA)
                .addFunction("FindFirstAncestor", &Instance::FindFirstAncestor)
                .addFunction("FindFirstAncestorOfClass", &Instance::FindFirstAncestorOfClass)
                .addFunction("FindFirstAncestorWhichIsA", &Instance::FindFirstAncestorWhichIsA)
                .addFunction("GetDescendants", &Instance::GetDescendants)
                .addFunction("getDescendants", &Instance::GetDescendants)
                .addFunction("GetFullName", &Instance::GetFullName)
                .addFunction("IsA", &Instance::IsA)
                .addFunction("isA", &Instance::IsA)
                .addFunction("Clone", &Instance::Clone)
                .addFunction("clone", &Instance::Clone)
                .addFunction("Destroy", &Instance::Destroy)
                .addFunction("destroy", &Instance::Destroy)
                .addIndexMetaMethod(Instance::LuaIndex)
                .addNewIndexMetaMethod(Instance::LuaNewIndex)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .deriveClass<DataModel, Instance>("DataModel")
                .addFunction("GetService", static_cast<std::shared_ptr<Instance>(DataModel::*)(const std::string&)>(&DataModel::GetService))
                .addFunction("FindService", static_cast<std::shared_ptr<Instance>(DataModel::*)(const std::string&)>(&DataModel::GetService))
            .endClass();

        luabridge::getGlobalNamespace(L).deriveClass<Workspace, Instance>("Workspace").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Lighting, Instance>("Lighting").endClass();
        luabridge::getGlobalNamespace(L)
            .deriveClass<BasePart, Instance>("BasePart")
                .addFunction("BreakJoints", &BasePart::BreakJoints)
                .addFunction("GetVelocity", &BasePart::GetVelocity)
                .addFunction("SetVelocity", &BasePart::SetVelocity)
            .endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Part, BasePart>("Part").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Model, Instance>("Model").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<JointInstance, Instance>("JointInstance").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<AutoJoint, JointInstance>("AutoJoint").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Weld, JointInstance>("Weld").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Snap, JointInstance>("Snap").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Glue, JointInstance>("Glue").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Motor, JointInstance>("Motor").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Hinge, JointInstance>("Hinge")
            .addFunction("GetCurrentAngle", &Hinge::GetCurrentAngle)
        .endClass();
        luabridge::getGlobalNamespace(L).deriveClass<VelocityMotor, JointInstance>("VelocityMotor")
            .addFunction("GetCurrentAngle", &VelocityMotor::GetCurrentAngle)
            .addFunction("SetTargetVelocity", &VelocityMotor::SetTargetVelocity)
        .endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Script, Instance>("Script").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<LocalScript, Script>("LocalScript").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Explosion, Instance>("Explosion").endClass();
    }

    void ScriptContext::ProcessScheduledTasks() {
        double currentTime = GetGameTime();

        while (!g_taskQueue.empty()) {
            auto& task = g_taskQueue.front();
            if (task.wakeTime > currentTime) break;

            if (task.isDelay && !task.callback.isNil()) {
                task.callback();
            } else if (!task.coroutine.isNil()) {
                // Push the thread onto the stack and get its state
                task.coroutine.push();
                lua_State* co = lua_tothread(L, -1);
                lua_pop(L, 1);
                
                if (co) {
                    int status = lua_resume(co, L, 0);
                    if (status != LUA_OK && status != LUA_YIELD) {
                        std::cerr << "Coroutine error: " << lua_tostring(co, -1) << std::endl;
                        lua_pop(co, 1);
                    }
                }
            }

            g_taskQueue.pop();
        }
    }

    void ScriptContext::SetDataModel(std::shared_ptr<DataModel> dataModel) {
        luabridge::setGlobal(L, dataModel, "game");

        auto workspace = dataModel->GetService<Workspace>();
        luabridge::setGlobal(L, workspace, "workspace");
    }

    void ScriptContext::Execute(const std::string& source, const std::string& chunkName) {
        Luau::CompileOptions options;
        std::string bytecode = Luau::compile(source, options);

        if (luau_load(L, chunkName.c_str(), bytecode.data(), bytecode.size(), 0) == 0) {
            if (lua_pcall(L, 0, 0, 0) != 0) {
                std::cerr << "Script Error: [string \"" << chunkName << "\"]:" << lua_tostring(L, -1) << std::endl;
                lua_pop(L, 1);
            }
        } else {
            std::cerr << "Compile Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
    }
}
