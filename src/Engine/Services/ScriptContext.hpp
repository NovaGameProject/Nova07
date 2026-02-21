// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include <lua.h>
#include <lualib.h>

// Avoid Luau macro / LuaBridge helper collision
#ifndef LUABRIDGE_USING_LUAU
#define LUABRIDGE_USING_LUAU
#endif
#undef lua_rawgetp
#undef lua_rawsetp
#include <luabridge3/LuaBridge/LuaBridge.h>

#include <string>
#include <memory>

namespace Nova {
    class DataModel;

    class ScriptContext : public Instance {
    public:
        ScriptContext();
        ~ScriptContext();

        NOVA_OBJECT_NO_PROPS(ScriptContext)

        void Execute(const std::string& source, const std::string& chunkName = "script");
        void SetDataModel(std::shared_ptr<DataModel> dataModel);

        lua_State* GetLuaState() { return L; }
        void ProcessScheduledTasks();

    private:
        void InitializeVM();
        void BindAPI();

        lua_State* L;
    };
}
