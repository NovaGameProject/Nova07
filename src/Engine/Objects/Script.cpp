// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Script.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/ScriptContext.hpp"

namespace Nova {
    Script::Script(std::string name) : Instance(name) {}

    void Script::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);

        if (m_hasRun || props.Disabled) return;

        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                Run();
            }
        }
    }

    void Script::Run() {
        if (m_hasRun) return;

        auto dm = GetDataModel();
        if (!dm) return;

        auto scriptContext = dm->GetService<ScriptContext>();
        if (!scriptContext) return;

        lua_State* L = scriptContext->GetLuaState();

        luabridge::setGlobal(L, std::static_pointer_cast<Script>(shared_from_this()), "script");

        scriptContext->Execute(props.Source, GetName());

        m_hasRun = true;
    }
}
