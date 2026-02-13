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
#include "Engine/Reflection/InstanceFactory.hpp"
#include "Common/MathTypes.hpp"
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include <iostream>

namespace Nova {

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

    ScriptContext::ScriptContext() : Instance("ScriptContext") {
        InitializeVM();
    }

    ScriptContext::~ScriptContext() {
        if (L) lua_close(L);
    }

    void ScriptContext::InitializeVM() {
        L = luaL_newstate();
        luaL_openlibs(L);

        // Replace print with our own that goes to stdout
        lua_pushcfunction(L, lua_print, "print");
        lua_setglobal(L, "print");

        BindAPI();
    }

    void ScriptContext::BindAPI() {
        // Individual blocks to ensure clean registration
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
                                
                                // Cache refresh
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
                .addIndexMetaMethod(Instance::LuaIndex)
                .addNewIndexMetaMethod(Instance::LuaNewIndex)
            .endClass();

        luabridge::getGlobalNamespace(L)
            .deriveClass<DataModel, Instance>("DataModel")
                .addFunction("GetService", static_cast<std::shared_ptr<Instance>(DataModel::*)(const std::string&)>(&DataModel::GetService))
            .endClass();

        luabridge::getGlobalNamespace(L).deriveClass<Workspace, Instance>("Workspace").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Lighting, Instance>("Lighting").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<BasePart, Instance>("BasePart").endClass();
        luabridge::getGlobalNamespace(L).deriveClass<Part, BasePart>("Part").endClass();
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
