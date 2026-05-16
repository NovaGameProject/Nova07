// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Instance.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/InstanceFactory.hpp"
#include "Engine/Reflection/TypeMarshaling.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/NetworkService.hpp"
#include "Common/Log.hpp"

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

        if (wasInWS && oldWS) oldWS->RefreshCachedParts();

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

        // 1. Special cases
        if (skey == "Name") return luabridge::LuaRef(L, self.GetName());
        if (skey == "Parent") return luabridge::LuaRef(L, self.GetParent());
        if (skey == "ClassName") return luabridge::LuaRef(L, self.GetClassName());

        // 1b. Derived properties
        if (skey == "Position") {
            auto* desc = ClassDescriptor::Get(self.GetClassName());
            while (desc) {
                if (auto it = desc->properties.find("CFrame"); it != desc->properties.end()) {
                    PropertyValue val = it->second->get(&self);
                    if (val.isCFrame()) {
                        return luabridge::LuaRef(L, val.toCFrame().position);
                    }
                    break;
                }
                desc = desc->baseClass;
            }
        }

        // 2. Walk the ClassDescriptor chain
        auto* desc = ClassDescriptor::Get(self.GetClassName());
        while (desc) {
            // Check properties
            if (auto it = desc->properties.find(skey); it != desc->properties.end()) {
                PropertyValue val = it->second->get(&self);
                return propertyValueToLua(L, val);
            }

            // Check methods
            if (auto it = desc->methods.find(skey); it != desc->methods.end()) {
                // Create a callable function that captures the instance and method
                auto methodCall = it->second.call;
                return luabridge::LuaRef(L, [inst = &self, methodCall](lua_State* L) -> int {
                    return methodCall(L, inst);
                });
            }

            // Check signals
            if (auto it = desc->signals.find(skey); it != desc->signals.end()) {
                Signal* sig = it->second.getter(&self);
                return luabridge::LuaRef(L, sig);
            }

            // Walk up inheritance
            desc = desc->baseClass;
        }

        // 3. Check children by name
        for (auto& child : self.children) {
            if (child->GetName() == skey) {
                return luabridge::LuaRef(L, child);
            }
        }

        return luabridge::LuaRef(L);
    }

    luabridge::LuaRef Instance::LuaNewIndex(Instance& self, const luabridge::LuaRef& key, const luabridge::LuaRef& value, lua_State* L) {
        if (!key.isString()) return luabridge::LuaRef(L);
        std::string skey = key.unsafe_cast<std::string>();

        // 1. Special case: Name
        if (skey == "Name") {
            if (value.isString()) {
                self.m_debugName = value.unsafe_cast<std::string>();
            }
            return luabridge::LuaRef(L);
        }

        // 2. Special case: Parent
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

        // 3. Special case: Position
        if (skey == "Position") {
            luabridge::push(L, value);
            if (luabridge::Stack<Vector3>::isInstance(L, -1)) {
                auto result = luabridge::Stack<Vector3>::get(L, -1);
                if (result) {
                    Vector3 pos = result.value();
                    auto* desc = ClassDescriptor::Get(self.GetClassName());
                    while (desc) {
                        // First try: find CFrame and update its position (for BasePart, etc.)
                        if (auto it = desc->properties.find("CFrame"); it != desc->properties.end()) {
                            PropertyValue cfVal = it->second->get(&self);
                            if (cfVal.isCFrame()) {
                                CFrame cf = cfVal.toCFrame();
                                cf.position = pos;
                                it->second->set(&self, PropertyValue(cf));
                                self.OnPropertyChanged("CFrame");

                                // Notify NetworkService
                                if (self.networkID != 0) {
                                    if (auto dm = self.GetDataModel()) {
                                        if (auto network = dm->GetService<NetworkService>()) {
                                            network->MarkDirty(&self, "CFrame");
                                        }
                                    }
                                }
                            }
                            lua_pop(L, 1);
                            return luabridge::LuaRef(L);
                        }
                        // Second try: find Position directly (for Explosion, etc.)
                        if (auto it = desc->properties.find("Position"); it != desc->properties.end()) {
                            it->second->set(&self, PropertyValue(pos));
                            self.OnPropertyChanged("Position");

                            // Notify NetworkService
                            if (self.networkID != 0) {
                                if (auto dm = self.GetDataModel()) {
                                    if (auto network = dm->GetService<NetworkService>()) {
                                        network->MarkDirty(&self, "Position");
                                    }
                                }
                            }
                            lua_pop(L, 1);
                            return luabridge::LuaRef(L);
                        }
                        desc = desc->baseClass;
                    }
                }
            }
            lua_pop(L, 1);
            return luabridge::LuaRef(L);
        }

        // 4. Convert value to PropertyValue
        PropertyValue propVal = luaToPropertyValue(value);

        // 5. Walk the ClassDescriptor chain to find and set the property
        auto* desc = ClassDescriptor::Get(self.GetClassName());
        while (desc) {
            if (auto it = desc->properties.find(skey); it != desc->properties.end()) {
                if (it->second->set(&self, propVal)) {
                    self.OnPropertyChanged(skey);

                    // Notify NetworkService if this is a replicated property
                    if (self.networkID != 0 && desc->replicatedProperties.contains(skey)) {
                        if (auto dm = self.GetDataModel()) {
                            if (auto network = dm->GetService<NetworkService>()) {
                                network->MarkDirty(&self, skey);
                            }
                        }
                    }
                } else {
                    LOG_WRN("Instance", "Failed to set property '%s' on %s", skey.c_str(), self.GetClassName().c_str());
                }
                return luabridge::LuaRef(L);
            }
            desc = desc->baseClass;
        }

        // Silently ignore unknown properties — Lua scripts may set arbitrary keys
        return luabridge::LuaRef(L);
    }

    std::shared_ptr<Instance> Instance::FindFirstChild(const std::string& name, bool recursive) {
        for (auto& child : children) {
            if (child->GetName() == name) return child;
        }
        if (recursive) {
            for (auto& child : children) {
                if (auto found = child->FindFirstChild(name, true)) return found;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Instance> Instance::FindFirstChildOfClass(const std::string& className) {
        for (auto& child : children) {
            if (child->GetClassName() == className) return child;
        }
        return nullptr;
    }

    std::shared_ptr<Instance> Instance::FindFirstChildWhichIsA(const std::string& className, bool recursive) {
        for (auto& child : children) {
            if (child->IsA(className)) return child;
        }
        if (recursive) {
            for (auto& child : children) {
                if (auto found = child->FindFirstChildWhichIsA(className, true)) return found;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Instance> Instance::FindFirstAncestor(const std::string& name) {
        auto p = parent.lock();
        while (p) {
            if (p->GetName() == name) return p;
            p = p->parent.lock();
        }
        return nullptr;
    }

    std::shared_ptr<Instance> Instance::FindFirstAncestorOfClass(const std::string& className) {
        auto p = parent.lock();
        while (p) {
            if (p->GetClassName() == className) return p;
            p = p->parent.lock();
        }
        return nullptr;
    }

    std::shared_ptr<Instance> Instance::FindFirstAncestorWhichIsA(const std::string& className) {
        auto p = parent.lock();
        while (p) {
            if (p->IsA(className)) return p;
            p = p->parent.lock();
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Instance>> Instance::GetDescendants() {
        std::vector<std::shared_ptr<Instance>> result;
        for (auto& child : children) {
            result.push_back(child);
            auto childDescendants = child->GetDescendants();
            result.insert(result.end(), childDescendants.begin(), childDescendants.end());
        }
        return result;
    }

    std::string Instance::GetFullName() {
        std::string result = GetName();
        auto p = parent.lock();
        while (p && !std::dynamic_pointer_cast<DataModel>(p)) {
            result = p->GetName() + "." + result;
            p = p->parent.lock();
        }
        return result;
    }

    bool Instance::IsA(const std::string& className) {
        auto* desc = ClassDescriptor::Get(GetClassName());
        if (desc) return desc->IsA(className);
        return GetClassName() == className;
    }

    std::shared_ptr<Instance> Instance::Clone() {
        auto inst = InstanceFactory::Get().Create(GetClassName());
        if (!inst) return nullptr;

        // Copy properties via ClassDescriptor
        auto* desc = ClassDescriptor::Get(GetClassName());
        if (desc) {
            for (auto& [name, accessor] : desc->properties) {
                PropertyValue val = accessor->get(this);
                accessor->set(inst.get(), val);
            }
        }

        for (auto& child : children) {
            auto childClone = child->Clone();
            if (childClone) {
                childClone->SetParent(inst);
            }
        }

        return inst;
    }

    void Instance::Destroy() {
        if (m_destroyed) return;
        m_destroyed = true;

        // Notify NetworkService to broadcast destroy to clients
        if (networkID != 0) {
            if (auto dm = GetDataModel()) {
                if (auto network = dm->GetService<NetworkService>()) {
                    network->BroadcastDestroyObject(networkID);
                }
            }
        }

        auto childrenCopy = children;
        for (auto& child : childrenCopy) {
            child->Destroy();
        }
        children.clear();

        SetParent(nullptr);
    }
}
