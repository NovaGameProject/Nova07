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
#include "Engine/Objects/InstanceFactory.hpp"
#include "Common/Log.hpp"
#include <algorithm>

namespace Nova {

    NetworkService::NetworkService() : Instance("NetworkService") {
        if (enet_initialize() != 0) {
            LOG_ERR("Network", "Failed to initialize ENet");
        }
    }

    NetworkService::~NetworkService() {
        StopServer();
        Disconnect();
        enet_deinitialize();
    }

    // ===== Network Thread =====
    void NetworkService::NetworkThreadFunc() {
        while (!mStopping) {
            // Process ENet events
            if (mHost) {
                ENetEvent event;
                while (enet_host_service(mHost, &event, 0) > 0) {
                    switch (event.type) {
                        case ENET_EVENT_TYPE_CONNECT: {
                            // Queue connect event
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;
                            pkt.data = {0x01}; // Connect marker
                            mIncomingPackets.push(std::move(pkt));
                            break;
                        }
                        case ENET_EVENT_TYPE_RECEIVE: {
                            // Queue received packet
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;
                            pkt.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                            mIncomingPackets.push(std::move(pkt));
                            enet_packet_destroy(event.packet);
                            break;
                        }
                        case ENET_EVENT_TYPE_DISCONNECT: {
                            // Queue disconnect event
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;
                            pkt.data = {0x02}; // Disconnect marker
                            mIncomingPackets.push(std::move(pkt));
                            break;
                        }
                        default:
                            break;
                    }
                }

                // Send queued outgoing packets
                {
                    std::lock_guard<std::mutex> lock(mOutgoingMutex);
                    while (!mOutgoingPackets.empty()) {
                        auto& out = mOutgoingPackets.front();
                        uint32_t flags = out.reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
                        ENetPacket* packet = enet_packet_create(out.data.data(), out.data.size(), flags);
                        enet_peer_send(out.target, 0, packet);
                        mOutgoingPackets.pop();
                    }
                }

                enet_host_flush(mHost);
            }

            // Sleep to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    // ===== Server Mode =====
    bool NetworkService::StartServer(uint16_t port) {
        if (mIsServer) return false;

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        mHost = enet_host_create(&address, 32, 2, 0, 0);
        if (!mHost) {
            LOG_ERR("Network", "Failed to create ENet server host on port %u", port);
            return false;
        }

        mIsServer = true;
        mStopping = false;
        mNetworkThread = std::thread(&NetworkService::NetworkThreadFunc, this);

        LOG_INF("Network", "Server started on port %u", port);
        return true;
    }

    void NetworkService::StopServer() {
        if (!mIsServer) return;

        mStopping = true;
        if (mNetworkThread.joinable()) mNetworkThread.join();

        mPlayers.clear();
        mPeerToPlayer.clear();
        mPendingSyncs.clear();

        if (mHost) {
            enet_host_destroy(mHost);
            mHost = nullptr;
        }

        mIsServer = false;
        LOG_INF("Network", "Server stopped");
    }

    // ===== Client Mode =====
    bool NetworkService::ConnectToServer(const std::string& host, uint16_t port) {
        if (mIsClient) return false;

        mHost = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!mHost) {
            LOG_ERR("Network", "Failed to create ENet client host");
            return false;
        }

        ENetAddress address;
        enet_address_set_host(&address, host.c_str());
        address.port = port;

        mServerPeer = enet_host_connect(mHost, &address, 2, 0);
        if (!mServerPeer) {
            LOG_ERR("Network", "Failed to connect to %s:%u", host.c_str(), port);
            enet_host_destroy(mHost);
            mHost = nullptr;
            return false;
        }

        mIsClient = true;
        mStopping = false;
        mNetworkThread = std::thread(&NetworkService::NetworkThreadFunc, this);

        LOG_INF("Network", "Connecting to %s:%u...", host.c_str(), port);
        return true;
    }

    void NetworkService::Disconnect() {
        if (!mIsClient) return;

        mStopping = true;
        if (mNetworkThread.joinable()) mNetworkThread.join();

        if (mServerPeer) {
            enet_peer_disconnect(mServerPeer, 0);
            mServerPeer = nullptr;
        }

        if (mHost) {
            enet_host_destroy(mHost);
            mHost = nullptr;
        }

        mIsClient = false;
        mClientInstances.clear();
        LOG_INF("Network", "Disconnected");
    }

    // ===== Main Thread Tick =====
    void NetworkService::Tick(float dt) {
        if (!mIsServer && !mIsClient) return;

        // Process incoming packets from network thread
        ProcessIncomingPackets();

        // Process incremental sync queue (server)
        if (mIsServer) {
            ProcessSyncQueue();
            ReplicateDirtyProperties(dt);
        }
    }

    void NetworkService::ProcessIncomingPackets() {
        std::queue<IncomingPacket> packets;
        {
            std::lock_guard<std::mutex> lock(mIncomingMutex);
            packets.swap(mIncomingPackets);
        }

        // Limit packets per frame to avoid freezing
        static constexpr size_t MAX_PACKETS_PER_FRAME = 20;
        size_t processed = 0;
        bool needsRefresh = false;

        while (!packets.empty() && processed < MAX_PACKETS_PER_FRAME) {
            auto& pkt = packets.front();

            if (pkt.data.size() == 1 && pkt.data[0] == 0x01) {
                HandleConnect(pkt.sender);
            } else if (pkt.data.size() == 1 && pkt.data[0] == 0x02) {
                HandleDisconnect(pkt.sender);
            } else if (!pkt.data.empty()) {
                PacketReader reader(pkt.data.data(), pkt.data.size());
                PacketType type = static_cast<PacketType>(reader.ReadU8());

                switch (type) {
                    case PacketType::CreateObject:
                        HandleCreateObject(pkt.sender, reader);
                        needsRefresh = true;
                        break;
                    case PacketType::DestroyObject:
                        HandleDestroyObject(pkt.sender, reader);
                        needsRefresh = true;
                        break;
                    case PacketType::PropertyUpdate:
                        HandlePropertyUpdate(pkt.sender, reader);
                        break;
                    case PacketType::FullSync: {
                        NetworkID workspaceID = reader.ReadU32();
                        auto dm = GetDataModel();
                        if (dm) {
                            auto ws = dm->GetService<Workspace>();
                            ws->networkID = workspaceID;
                            mClientInstances[workspaceID] = ws;
                            LOG_INF("Network", "FullSync: Workspace ID = %u", workspaceID);
                        }
                        break;
                    }
                    case PacketType::PlayerJoin: {
                        uint32_t playerID = reader.ReadU32();
                        std::string playerName = reader.ReadString();
                        LOG_INF("Network", "Player joined: %s (ID: %u)", playerName.c_str(), playerID);
                        break;
                    }
                    case PacketType::PlayerLeave: {
                        uint32_t playerID = reader.ReadU32();
                        LOG_INF("Network", "Player left: ID %u", playerID);
                        break;
                    }
                    default:
                        break;
                }
            }

            packets.pop();
            processed++;
        }

        // Put unprocessed packets back
        if (!packets.empty()) {
            std::lock_guard<std::mutex> lock(mIncomingMutex);
            while (!packets.empty()) {
                mIncomingPackets.push(std::move(packets.front()));
                packets.pop();
            }
        }

        // Single refresh after batch
        if (needsRefresh) {
            if (auto dm = GetDataModel()) {
                if (auto ws = dm->GetService<Workspace>()) {
                    ws->RefreshCachedParts();
                }
            }
        }
    }

    void NetworkService::ProcessSyncQueue() {
        static constexpr size_t SYNC_PER_TICK = 5;

        for (auto it = mPendingSyncs.begin(); it != mPendingSyncs.end();) {
            auto& sync = *it;
            size_t sent = 0;

            while (sync.nextIndex < sync.objects.size() && sent < SYNC_PER_TICK) {
                Instance* obj = sync.objects[sync.nextIndex];
                if (obj && obj->networkID != 0) {
                    SendCreateObject(sync.peer, obj);
                    SendAllProperties(sync.peer, obj, obj->networkID);
                }
                sync.nextIndex++;
                sent++;
            }

            if (sync.nextIndex >= sync.objects.size()) {
                LOG_INF("Network", "FullSync complete for client");
                it = mPendingSyncs.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ===== Connection Handling =====
    void NetworkService::HandleConnect(ENetPeer* peer) {
        auto player = std::make_shared<Player>("Player_" + std::to_string(peer->connectID),
                                                peer->connectID);
        mPlayers.push_back(player);
        mPeerToPlayer[peer] = player;

        auto dm = GetDataModel();
        if (dm) {
            player->SetParent(dm);
        }

        LOG_INF("Network", "Player connected: %s (ID: %u)", player->GetName().c_str(), peer->connectID);

        // Send welcome packet
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::PlayerJoin));
        writer.WriteU32(player->playerID);
        writer.WriteString(player->playerName);
        QueueSend(peer, writer);

        // Queue full world state
        SendFullSync(peer);
    }

    void NetworkService::HandleDisconnect(ENetPeer* peer) {
        auto it = mPeerToPlayer.find(peer);
        if (it != mPeerToPlayer.end()) {
            auto player = it->second;
            LOG_INF("Network", "Player disconnected: %s", player->GetName().c_str());

            mPlayers.erase(std::remove(mPlayers.begin(), mPlayers.end(), player), mPlayers.end());
            mPeerToPlayer.erase(it);

            // Broadcast to other clients
            PacketWriter writer;
            writer.WriteU8(static_cast<uint8_t>(PacketType::PlayerLeave));
            writer.WriteU32(player->playerID);
            for (auto& [otherPeer, otherPlayer] : mPeerToPlayer) {
                if (otherPeer != peer) {
                    QueueSend(otherPeer, writer);
                }
            }

            player->Destroy();
        }
    }

    // ===== Packet Handlers =====
    void NetworkService::HandleCreateObject(ENetPeer* sender, PacketReader& reader) {
        NetworkID networkID = reader.ReadU32();
        NetworkID parentNetworkID = reader.ReadU32();
        std::string className = reader.ReadString();
        std::string name = reader.ReadString();

        auto instance = InstanceFactory::Get().Create(className);
        if (!instance) {
            LOG_WRN("Network", "Failed to create instance of class '%s'", className.c_str());
            return;
        }

        instance->networkID = networkID;
        instance->m_debugName = name;

        // Find parent
        if (parentNetworkID != 0) {
            auto parentIt = mClientInstances.find(parentNetworkID);
            if (parentIt != mClientInstances.end()) {
                instance->SetParent(parentIt->second);
            } else {
                auto dm = GetDataModel();
                if (dm) {
                    auto ws = dm->GetService<Workspace>();
                    if (ws) instance->SetParent(ws);
                }
            }
        } else {
            auto dm = GetDataModel();
            if (dm) {
                auto ws = dm->GetService<Workspace>();
                if (ws) instance->SetParent(ws);
            }
        }

        mClientInstances[networkID] = instance;
    }

    void NetworkService::HandleDestroyObject(ENetPeer* sender, PacketReader& reader) {
        NetworkID networkID = reader.ReadU32();

        auto it = mClientInstances.find(networkID);
        if (it != mClientInstances.end()) {
            it->second->Destroy();
            mClientInstances.erase(it);
        }
    }

    void NetworkService::HandlePropertyUpdate(ENetPeer* sender, PacketReader& reader) {
        NetworkID networkID = reader.ReadU32();
        std::string propertyName = reader.ReadString();
        PropertyValue value = ReadPropertyValue(reader);

        auto it = mClientInstances.find(networkID);
        if (it == mClientInstances.end()) return;

        auto instance = it->second;
        if (!instance) {
            mClientInstances.erase(it);
            return;
        }

        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (!desc) return;

        auto* accessor = desc->FindProperty(propertyName);
        if (accessor) {
            accessor->set(instance.get(), value);
        }
    }

    void NetworkService::HandleRemoteEvent(ENetPeer* sender, PacketReader& reader) {
        // TODO: Fire remote event
    }

    // ===== Server: Full Sync =====
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

        // Send Workspace ID
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::FullSync));
        writer.WriteU32(workspace->networkID);
        QueueSend(peer, writer);

        // Queue objects for incremental sending
        PendingSync sync;
        sync.peer = peer;
        sync.nextIndex = 0;

        std::function<void(Instance*)> collect = [&](Instance* inst) {
            if (!inst || inst->networkID == 0) return;
            sync.objects.push_back(inst);
            for (auto& child : inst->GetChildren()) {
                collect(child.get());
            }
        };

        for (auto& child : workspace->GetChildren()) {
            collect(child.get());
        }

        mPendingSyncs.push_back(std::move(sync));
        LOG_INF("Network", "FullSync queued: %zu objects", mPendingSyncs.back().objects.size());
    }

    void NetworkService::SendAllProperties(ENetPeer* peer, Instance* instance, NetworkID id) {
        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (!desc) return;

        const ClassDescriptor* current = desc;
        while (current) {
            for (auto& [name, accessor] : current->properties) {
                PropertyValue value = accessor->get(instance);
                SendPropertyUpdate(peer, id, name, value);
            }
            current = current->baseClass;
        }
    }

    // ===== Packet Sending (queues to network thread) =====
    void NetworkService::QueueSend(ENetPeer* peer, const PacketWriter& writer, bool reliable) {
        std::lock_guard<std::mutex> lock(mOutgoingMutex);
        OutgoingPacket out;
        out.target = peer;
        out.data = writer.GetData();
        out.reliable = reliable;
        mOutgoingPackets.push(std::move(out));
    }

    void NetworkService::SendCreateObject(ENetPeer* peer, Instance* instance) {
        if (!instance || instance->networkID == 0) return;

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

        QueueSend(peer, writer);
    }

    void NetworkService::SendDestroyObject(ENetPeer* peer, NetworkID id) {
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::DestroyObject));
        writer.WriteU32(id);
        QueueSend(peer, writer);
    }

    void NetworkService::SendPropertyUpdate(ENetPeer* peer, NetworkID id, const std::string& prop, const PropertyValue& value) {
        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::PropertyUpdate));
        writer.WriteU32(id);
        writer.WriteString(prop);
        WritePropertyValue(writer, value);
        QueueSend(peer, writer);
    }

    // ===== Property Serialization =====
    void NetworkService::WritePropertyValue(PacketWriter& writer, const PropertyValue& value) {
        writer.WriteU8(static_cast<uint8_t>(value.kind));

        switch (value.kind) {
            case PropertyValue::Kind::Nil:
                break;
            case PropertyValue::Kind::Bool:
                writer.WriteU8(value.toBool() ? 1 : 0);
                break;
            case PropertyValue::Kind::Int:
                writer.WriteU32(static_cast<uint32_t>(value.toInt()));
                break;
            case PropertyValue::Kind::Float:
                writer.WriteFloat(static_cast<float>(value.toFloat()));
                break;
            case PropertyValue::Kind::String:
                writer.WriteString(value.toString());
                break;
            case PropertyValue::Kind::Vector3: {
                auto v = value.toVector3();
                writer.WriteVec3(v);
                break;
            }
            case PropertyValue::Kind::CFrame: {
                auto cf = value.toCFrame();
                writer.WriteVec3(cf.position);
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        writer.WriteFloat(cf.rotation[i][j]);
                    }
                }
                break;
            }
            case PropertyValue::Kind::Color3: {
                auto c = value.toColor3();
                writer.WriteFloat(c.r);
                writer.WriteFloat(c.g);
                writer.WriteFloat(c.b);
                break;
            }
        }
    }

    PropertyValue NetworkService::ReadPropertyValue(PacketReader& reader) {
        uint8_t kind = reader.ReadU8();

        switch (static_cast<PropertyValue::Kind>(kind)) {
            case PropertyValue::Kind::Nil:
                return PropertyValue();
            case PropertyValue::Kind::Bool:
                return PropertyValue(reader.ReadU8() != 0);
            case PropertyValue::Kind::Int:
                return PropertyValue(static_cast<int64_t>(reader.ReadU32()));
            case PropertyValue::Kind::Float:
                return PropertyValue(static_cast<double>(reader.ReadFloat()));
            case PropertyValue::Kind::String:
                return PropertyValue(reader.ReadString());
            case PropertyValue::Kind::Vector3:
                return PropertyValue(reader.ReadVec3());
            case PropertyValue::Kind::CFrame: {
                CFrame cf;
                cf.position = reader.ReadVec3();
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        cf.rotation[i][j] = reader.ReadFloat();
                    }
                }
                return PropertyValue(cf);
            }
            case PropertyValue::Kind::Color3: {
                Color3 c;
                c.r = reader.ReadFloat();
                c.g = reader.ReadFloat();
                c.b = reader.ReadFloat();
                return PropertyValue::FromColor3(c);
            }
            default:
                return PropertyValue();
        }
    }

    // ===== Dirty Property Replication =====
    void NetworkService::MarkDirty(Instance* inst, const std::string& prop) {
        if (!mIsServer || inst->networkID == 0) return;

        std::lock_guard<std::mutex> lock(mDirtyMutex);
        mDirtyProperties.push_back({inst->networkID, prop, 0.0f});
    }

    void NetworkService::ReplicateDirtyProperties(float dt) {
        if (!mIsServer) return;

        std::vector<DirtyProperty> dirty;
        {
            std::lock_guard<std::mutex> lock(mDirtyMutex);
            dirty.swap(mDirtyProperties);
        }

        for (auto& prop : dirty) {
            auto* instance = mIDRegistry.GetInstance(prop.targetID);
            if (!instance) continue;

            auto* desc = ClassDescriptor::Get(instance->GetClassName());
            if (!desc) continue;

            auto* accessor = desc->FindProperty(prop.propertyName);
            if (!accessor) continue;

            PropertyValue value = accessor->get(instance);

            for (auto& [peer, player] : mPeerToPlayer) {
                SendPropertyUpdate(peer, prop.targetID, prop.propertyName, value);
            }
        }
    }

}
