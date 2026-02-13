// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Instance.hpp"
#include "Engine/Reflection/TypeMarshaling.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include <iostream>

namespace Nova {
    std::shared_ptr<DataModel> Instance::GetDataModel() {
        if (auto dm = std::dynamic_pointer_cast<DataModel>(shared_from_this())) {
            return dm;
        }
        if (auto p = parent.lock()) {
            return p->GetDataModel();
        }
        return nullptr;
    }

    bool Instance::IsDescendantOf(std::shared_ptr<Instance> other) {
        if (!other) return false;
        if (other.get() == this) return false;
        auto p = parent.lock();
        while (p) {
            if (p == other) return true;
            p = p->parent.lock();
        }
        return false;
    }

    void Instance::SetParent(std::shared_ptr<Instance> newParent) {
        auto self = shared_from_this();
        
        // Find old workspace for refresh
        std::shared_ptr<Workspace> oldWS = nullptr;
        if (auto dm = GetDataModel()) oldWS = dm->GetService<Workspace>();
        bool wasInWS = oldWS && (this == oldWS.get() || IsDescendantOf(oldWS));

        if (auto p = parent.lock()) {
            auto& c = p->children;
            c.erase(std::remove(c.begin(), c.end(), self), c.end());
        }
        
        parent = newParent;
        if (newParent) {
            newParent->children.push_back(self);
        }
        
        OnAncestorChanged(self, newParent);

        // Refresh old workspace cache
        if (wasInWS && oldWS) oldWS->RefreshCachedParts();

        // Refresh new workspace cache
        if (newParent) {
            if (auto dm = newParent->GetDataModel()) {
                if (auto newWS = dm->GetService<Workspace>()) {
                    if (this == newWS.get() || IsDescendantOf(newWS)) {
                        newWS->RefreshCachedParts();
                    }
                }
            }
        }
    }

    void Instance::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        for (auto& child : children) {
            child->OnAncestorChanged(instance, newParent);
        }
    }

    luabridge::LuaRef Instance::LuaIndex(Instance& self, const luabridge::LuaRef& key, lua_State* L) {
        if (!key.isString()) return luabridge::LuaRef(L);
        std::string skey = key.unsafe_cast<std::string>();

        // Explicitly handle Name and Parent first to ensure they work for all instances
        if (skey == "Name") return luabridge::LuaRef(L, self.GetName());
        if (skey == "Parent") return luabridge::LuaRef(L, self.GetParent());
        if (skey == "ClassName") return luabridge::LuaRef(L, self.GetClassName());

        // Helper: Position (mapped from CFrame)
        if (skey == "Position") {
            rfl::Generic cfGen = self.GetProperty("CFrame");
            if (!std::holds_alternative<std::nullopt_t>(cfGen.variant())) {
                auto cfResult = rfl::from_generic<CFrameReflect, rfl::UnderlyingEnums>(cfGen);
                if (cfResult) {
                    return luabridge::LuaRef(L, Vector3(cfResult->x, cfResult->y, cfResult->z));
                }
            }
        }

        // 1. Try ClassDescriptor (Signals)
        std::string currentClass = self.GetClassName();
        while (!currentClass.empty()) {
            auto desc = ClassDescriptor::Get(currentClass);
            if (desc) {
                if (desc->signals.contains(skey)) {
                    Signal* sig = desc->signals[skey].getter(&self);
                    return luabridge::LuaRef(L, sig);
                }
            }
            if (desc) currentClass = desc->baseClassName;
            else break;
        }

        // 2. Try generic property system
        rfl::Generic prop = self.GetProperty(skey);
        if (!std::holds_alternative<std::nullopt_t>(prop.variant())) {
            return genericToLua(L, prop);
        }

        // 3. Try children
        for (auto& child : self.children) {
            if (child->GetName() == skey) {
                return luabridge::LuaRef(L, child);
            }
        }

        return luabridge::LuaRef(L); // Returns nil
    }

    luabridge::LuaRef Instance::LuaNewIndex(Instance& self, const luabridge::LuaRef& key, const luabridge::LuaRef& value, lua_State* L) {
        if (!key.isString()) return luabridge::LuaRef(L);
        std::string skey = key.unsafe_cast<std::string>();

        if (skey == "Parent") {
            if (value.isNil()) {
                self.SetParent(nullptr);
            } else {
                luabridge::push(L, value);
                if (luabridge::Stack<Instance>::isInstance(L, -1)) {
                    auto res = value.cast<std::shared_ptr<Instance>>();
                    if (res) {
                        self.SetParent(res.value());
                    }
                }
                lua_pop(L, 1);
            }
            return luabridge::LuaRef(L);
        }

        // Helper: Position (update CFrame translation)
        if (skey == "Position") {
            luabridge::push(L, value);
            if (luabridge::Stack<Vector3>::isInstance(L, -1)) {
                auto res = value.cast<Vector3>();
                if (res) {
                    Vector3 pos = res.value();
                    rfl::Generic cfGen = self.GetProperty("CFrame");
                    if (!std::holds_alternative<std::nullopt_t>(cfGen.variant())) {
                        auto cfResult = rfl::from_generic<CFrameReflect, rfl::UnderlyingEnums>(cfGen);
                        if (cfResult) {
                            CFrameReflect newCf = cfResult.value();
                            newCf.x = pos.x; newCf.y = pos.y; newCf.z = pos.z;
                            self.SetProperty("CFrame", rfl::to_generic<rfl::UnderlyingEnums>(newCf));
                        }
                    }
                }
            }
            lua_pop(L, 1);
            return luabridge::LuaRef(L);
        }

        rfl::Generic g = luaToGeneric(value);
        if (self.SetProperty(skey, g)) {
            return luabridge::LuaRef(L);
        }

        std::cerr << "Failed to set property " << skey << " on " << self.GetClassName() << std::endl;
        return luabridge::LuaRef(L);
    }
}
