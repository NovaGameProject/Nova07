// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>

namespace Nova {
    class Instance;

    using NetworkID = uint32_t;

    class NetworkIDRegistry {
    public:
        NetworkID Allocate() { return nextID++; }

        void Register(Instance* instance, NetworkID id) {
            instanceToID[instance] = id;
            idToInstance[id] = instance;
        }

        void Unregister(Instance* instance) {
            auto it = instanceToID.find(instance);
            if (it != instanceToID.end()) {
                idToInstance.erase(it->second);
                instanceToID.erase(it);
            }
        }

        void Unregister(NetworkID id) {
            auto it = idToInstance.find(id);
            if (it != idToInstance.end()) {
                instanceToID.erase(it->second);
                idToInstance.erase(it);
            }
        }

        NetworkID GetNetworkID(Instance* instance) const {
            auto it = instanceToID.find(instance);
            return it != instanceToID.end() ? it->second : 0;
        }

        Instance* GetInstance(NetworkID id) const {
            auto it = idToInstance.find(id);
            return it != idToInstance.end() ? it->second : nullptr;
        }

        bool HasInstance(Instance* instance) const {
            return instanceToID.contains(instance);
        }

        bool HasID(NetworkID id) const {
            return idToInstance.contains(id);
        }

        void Clear() {
            instanceToID.clear();
            idToInstance.clear();
        }

    private:
        NetworkID nextID = 1;  // 0 = invalid
        std::unordered_map<Instance*, NetworkID> instanceToID;
        std::unordered_map<NetworkID, Instance*> idToInstance;
    };
}
