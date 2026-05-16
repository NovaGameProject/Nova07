// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/NetworkService.hpp"
#include "Engine/Objects/Player.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Common/Log.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

    void NetworkService::AssignNetworkIDs(Instance* root) {
        if (!root) return;

        if (root->networkID == 0) {
            root->networkID = mIDRegistry.Allocate();
            mIDRegistry.Register(root, root->networkID);
        }

        for (auto& child : root->GetChildren()) {
            AssignNetworkIDs(child.get());
        }
    }

    void NetworkService::SendFullSync(ENetPeer* peer) {
        auto dm = GetDataModel();
        if (!dm) return;

        AssignNetworkIDs(dm.get());

        auto workspace = dm->GetService<Workspace>();
        if (!workspace) return;

        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::FullSync));
        writer.WriteU32(workspace->networkID);
        QueueSend(peer, writer, PacketType::FullSync);

        PendingSync sync;
        sync.peer = peer;
        sync.nextIndex = 0;

        std::function<void(std::shared_ptr<Instance>)> collect = [&](std::shared_ptr<Instance> inst) {
            if (!inst || inst->networkID == 0) return;
            sync.objects.push_back(inst);
            for (auto& child : inst->GetChildren()) {
                collect(child);
            }
        };

        for (auto& child : workspace->GetChildren()) {
            collect(child);
        }

        mPendingSyncs.push_back(std::move(sync));
        mPendingSyncs.back().isSyncing = true;
        LOG_INF("Network", "FullSync queued: %zu objects", mPendingSyncs.back().objects.size());
    }

    void NetworkService::SendReplicatedProperties(ENetPeer* peer, Instance* instance, NetworkID id) {
        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (!desc) return;

        std::vector<std::pair<std::string, PropertyValue>> properties;
        const ClassDescriptor* current = desc;
        while (current) {
            for (auto& name : current->replicatedProperties) {
                auto it = current->properties.find(name);
                if (it == current->properties.end()) continue;
                PropertyValue value = it->second->get(instance);
                properties.emplace_back(name, value);
            }
            current = current->baseClass;
        }

        if (!properties.empty()) {
            SendBatchProperties(peer, id, properties);
        }
    }

    void NetworkService::QueueSend(ENetPeer* peer, const PacketWriter& writer, PacketType type, bool reliable) {
        std::lock_guard<std::mutex> lock(mOutgoingMutex);

        if (mOutgoingPackets.size() >= MAX_OUTGOING_QUEUE) {
            if (type == PacketType::PropertyUpdate || type == PacketType::BatchPropertyUpdate) {
                size_t dropped = mDroppedPackets.fetch_add(1, std::memory_order_relaxed) + 1;
                if (dropped == 1 || dropped % 100 == 0) {
                    LOG_WRN("Network", "Backpressure: dropping property updates (queue=%zu, dropped=%zu)",
                        mOutgoingPackets.size(), dropped);
                }
                return;
            }
        }

        OutgoingPacket out;
        out.target = peer;
        out.data = writer.GetData();
        out.reliable = reliable;
        out.type = type;
        mOutgoingPackets.push(std::move(out));
    }

    void NetworkService::SendCreateObject(ENetPeer* peer, Instance* instance) {
        if (!instance || instance->networkID == 0) return;

        std::vector<std::pair<std::string, PropertyValue>> properties;
        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (desc) {
            const ClassDescriptor* current = desc;
            while (current) {
                for (auto& name : current->replicatedProperties) {
                    auto it = current->properties.find(name);
                    if (it == current->properties.end()) continue;
                    PropertyValue value = it->second->get(instance);
                    properties.emplace_back(name, value);
                    if (name == "CFrame" && value.isCFrame()) {
                        mLastSentCFrame[instance->networkID] = value.toCFrame();
                    }
                }
                current = current->baseClass;
            }
        }

        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::CreateObject));
        writer.WriteU32(instance->networkID);

        NetworkID parentID = 0;
        if (auto parent = instance->GetParent()) {
            parentID = parent->networkID;
        }
        writer.WriteU32(parentID);
        writer.WriteString(instance->GetClassName());
        writer.WriteString(instance->GetName());

        writer.WriteU16(static_cast<uint16_t>(properties.size()));
        for (auto& [name, value] : properties) {
            writer.WriteString(name);
            WritePropertyValue(writer, value);
        }

        QueueSend(peer, writer, PacketType::CreateObject);
    }

    void NetworkService::SendDestroyObject(ENetPeer* peer, NetworkID id) {
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::DestroyObject));
        writer.WriteU32(id);
        QueueSend(peer, writer, PacketType::DestroyObject);
    }

    void NetworkService::SendPropertyUpdate(ENetPeer* peer, NetworkID id, const std::string& prop, const PropertyValue& value) {
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::PropertyUpdate));
        writer.WriteU32(id);
        writer.WriteString(prop);
        WritePropertyValue(writer, value);
        if (prop == "CFrame" && value.isCFrame()) {
            mLastSentCFrame[id] = value.toCFrame();
        }
        bool reliable = !(prop == "CFrame" || prop == "Position");
        QueueSend(peer, writer, PacketType::PropertyUpdate, reliable);
    }

    void NetworkService::SendBatchProperties(ENetPeer* peer, NetworkID id, const std::vector<std::pair<std::string, PropertyValue>>& properties) {
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::BatchPropertyUpdate));
        writer.WriteU32(id);
        writer.WriteU16(static_cast<uint16_t>(properties.size()));
        bool hasCFrame = false;
        for (auto& [name, value] : properties) {
            writer.WriteString(name);
            WritePropertyValue(writer, value);
            if (name == "CFrame") hasCFrame = true;
        }
        // State snapshots: send unreliably if CFrame is present (most frequent, loss-tolerant)
        QueueSend(peer, writer, PacketType::BatchPropertyUpdate, !hasCFrame);
    }

    void NetworkService::SendBulkCFrameUpdate(ENetPeer* peer, const std::vector<std::pair<NetworkID, CFrame>>& updates) {
        if (updates.empty()) return;

        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::BulkCFrameUpdate));
        writer.WriteU16(static_cast<uint16_t>(updates.size()));
        for (auto& [id, cf] : updates) {
            writer.WriteU32(id);
            writer.WriteVec3(cf.position);
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    writer.WriteFloat(cf.rotation[i][j]);
                }
            }
        }
        // Unreliable — state snapshots, loss-tolerant
        QueueSend(peer, writer, PacketType::BulkCFrameUpdate, false);
    }

    void NetworkService::MarkDirty(Instance* inst, const std::string& prop) {
        if (!mIsServer || inst->networkID == 0) return;

        mMarkDirtyCalls.fetch_add(1, std::memory_order_relaxed);

        uint64_t key = (uint64_t(inst->networkID) << 32) | std::hash<std::string>{}(prop);
        mPendingProperties[key] = {inst->networkID, prop, inst->shared_from_this()};
    }

    void NetworkService::BroadcastDestroyObject(NetworkID id) {
        if (!mIsServer) return;

        for (auto it = mPendingProperties.begin(); it != mPendingProperties.end();) {
            if (it->second.targetID == id) {
                it = mPendingProperties.erase(it);
            } else {
                ++it;
            }
        }

        mLastSentCFrame.erase(id);

        for (auto& [peer, player] : mPeerToPlayer) {
            SendDestroyObject(peer, id);
        }

        mIDRegistry.Unregister(id);
    }

    void NetworkService::CheckForDrift() {
        auto dm = GetDataModel();
        if (!dm) return;
        auto ws = dm->GetService<Workspace>();
        if (!ws) return;

        auto* desc = ClassDescriptor::Get("BasePart");
        if (!desc) return;
        auto* cframeAccessor = desc->FindProperty("CFrame");
        if (!cframeAccessor) return;

        for (auto& part : ws->cachedParts) {
            if (part->networkID == 0) continue;

            auto it = mLastSentCFrame.find(part->networkID);
            if (it == mLastSentCFrame.end()) continue;

            const CFrame& currentCF = part->cframe;
            const CFrame& lastSent = it->second;

            float posDist = glm::distance(currentCF.position, lastSent.position);
            float rotDot = glm::abs(glm::dot(
                glm::quat_cast(currentCF.rotation),
                glm::quat_cast(lastSent.rotation)));

            if (posDist > 0.05f || rotDot < 0.999f) {
                MarkDirty(part.get(), "CFrame");
            }
        }
    }

    void NetworkService::DrainPendingProperties() {
        if (!mIsServer) return;
        if (mPendingProperties.empty()) return;

        std::unordered_map<uint64_t, DirtyProperty> pending;
        pending.swap(mPendingProperties);

        std::unordered_set<ENetPeer*> syncingPeers;
        for (auto& sync : mPendingSyncs) {
            if (sync.isSyncing) {
                syncingPeers.insert(sync.peer);
            }
        }

        // Separate CFrame updates (bulk) from other properties (per-part batch)
        struct BatchKey {
            ENetPeer* peer;
            NetworkID targetID;
            bool operator==(const BatchKey& o) const { return peer == o.peer && targetID == o.targetID; }
        };
        struct BatchKeyHash {
            size_t operator()(const BatchKey& k) const {
                return std::hash<ENetPeer*>{}(k.peer) ^ (std::hash<NetworkID>{}(k.targetID) << 16);
            }
        };
        std::unordered_map<ENetPeer*, std::vector<std::pair<NetworkID, CFrame>>> cframeBatches;
        std::unordered_map<BatchKey, std::vector<std::pair<std::string, PropertyValue>>, BatchKeyHash> otherBatches;

        for (auto& [key, prop] : pending) {
            auto instance = prop.instance;
            if (!instance || instance->IsDestroyed()) continue;

            auto* desc = ClassDescriptor::Get(instance->GetClassName());
            if (!desc) continue;

            auto* accessor = desc->FindProperty(prop.propertyName);
            if (!accessor) continue;

            PropertyValue value = accessor->get(instance.get());

            for (auto& [peer, player] : mPeerToPlayer) {
                if (syncingPeers.contains(peer)) {
                    for (auto& sync : mPendingSyncs) {
                        if (sync.peer == peer && sync.isSyncing) {
                            sync.queuedChanges.push_back(prop);
                            break;
                        }
                    }
                } else {
                    if (prop.propertyName == "CFrame" && value.isCFrame()) {
                        cframeBatches[peer].emplace_back(prop.targetID, value.toCFrame());
                    } else {
                        otherBatches[{peer, prop.targetID}].emplace_back(prop.propertyName, value);
                    }
                }
            }
        }

        // Update lastSentCFrame from the bulk batches
        for (auto& [peer, updates] : cframeBatches) {
            for (auto& [id, cf] : updates) {
                mLastSentCFrame[id] = cf;
            }
            SendBulkCFrameUpdate(peer, updates);
        }

        for (auto& [batchKey, properties] : otherBatches) {
            SendBatchProperties(batchKey.peer, batchKey.targetID, properties);
        }
    }

}
