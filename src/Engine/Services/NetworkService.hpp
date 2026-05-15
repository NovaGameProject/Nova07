// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/Objects/Instance.hpp"
#include "Engine/Networking/NetworkID.hpp"
#include "Engine/Networking/ReplicationProtocol.hpp"
#include "Common/PropertyValue.hpp"
#include <enet/enet.h>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>

namespace Nova {
    class Player;
    class BasePart;
    class ClassDescriptor;

    class NetworkService : public Instance {
    public:
        NetworkService();
        ~NetworkService();

        std::string GetClassName() const override { return "NetworkService"; }
        std::string GetName() const override { return m_debugName; }

        // Server mode
        bool StartServer(uint16_t port);
        void StopServer();

        // Client mode
        bool ConnectToServer(const std::string& host, uint16_t port);
        void Disconnect();

        // Called from main thread each frame
        void Tick(float dt);

        // State queries
        bool IsServer() const { return mIsServer; }
        bool IsClient() const { return mIsClient; }
        bool IsRunning() const { return mIsServer || mIsClient; }

        // Player management (server)
        const std::vector<std::shared_ptr<Player>>& GetPlayers() const { return mPlayers; }

        // NetworkID registry
        NetworkIDRegistry& GetIDRegistry() { return mIDRegistry; }

        // Mark a property as dirty for replication
        void MarkDirty(Instance* inst, const std::string& prop);

    private:
        ENetHost* mHost = nullptr;
        bool mIsServer = false;
        bool mIsClient = false;

        // Network thread
        std::thread mNetworkThread;
        std::atomic<bool> mStopping{false};

        // Server state
        std::vector<std::shared_ptr<Player>> mPlayers;
        std::unordered_map<ENetPeer*, std::shared_ptr<Player>> mPeerToPlayer;
        NetworkIDRegistry mIDRegistry;

        // Incremental sync queue (server side)
        struct PendingSync {
            ENetPeer* peer;
            std::vector<Instance*> objects;
            size_t nextIndex = 0;
        };
        std::vector<PendingSync> mPendingSyncs;

        // Client state
        ENetPeer* mServerPeer = nullptr;
        std::unordered_map<NetworkID, std::shared_ptr<Instance>> mClientInstances;

        // Thread-safe packet queue (network thread -> main thread)
        struct IncomingPacket {
            ENetPeer* sender;
            std::vector<uint8_t> data;
        };
        std::queue<IncomingPacket> mIncomingPackets;
        std::mutex mIncomingMutex;

        // Thread-safe send queue (main thread -> network thread)
        struct OutgoingPacket {
            ENetPeer* target;
            std::vector<uint8_t> data;
            bool reliable;
        };
        std::queue<OutgoingPacket> mOutgoingPackets;
        std::mutex mOutgoingMutex;

        // Dirty property tracking
        struct DirtyProperty {
            NetworkID targetID;
            std::string propertyName;
            float lastSentTime;
        };
        std::vector<DirtyProperty> mDirtyProperties;
        std::mutex mDirtyMutex;

        // Adaptive replication settings
        static constexpr float POSITION_THRESHOLD = 0.1f;
        static constexpr float ROTATION_THRESHOLD = 5.0f;
        static constexpr float VELOCITY_THRESHOLD = 1.0f;
        static constexpr float MAX_REPLICATION_RATE = 20.0f;
        static constexpr float MIN_REPLICATION_RATE = 5.0f;

        // Network thread entry point
        void NetworkThreadFunc();

        // Server: assign NetworkIDs to instance tree
        void AssignNetworkIDs(Instance* root);

        // Server: send full world state to a client
        void SendFullSync(ENetPeer* peer);

        // Server: send all replicated properties for an instance
        void SendAllProperties(ENetPeer* peer, Instance* instance, NetworkID id);

        // Helpers
        void ProcessIncomingPackets();
        void ProcessSyncQueue();
        void ReplicateDirtyProperties(float dt);

        // Packet sending (queues to network thread)
        void QueueSend(ENetPeer* peer, const PacketWriter& writer, bool reliable = true);
        void SendCreateObject(ENetPeer* peer, Instance* instance);
        void SendDestroyObject(ENetPeer* peer, NetworkID id);
        void SendPropertyUpdate(ENetPeer* peer, NetworkID id, const std::string& prop, const PropertyValue& value);

        // Packet handlers (called from main thread)
        void HandleConnect(ENetPeer* peer);
        void HandleDisconnect(ENetPeer* peer);
        void HandleCreateObject(ENetPeer* sender, PacketReader& reader);
        void HandleDestroyObject(ENetPeer* sender, PacketReader& reader);
        void HandlePropertyUpdate(ENetPeer* sender, PacketReader& reader);
        void HandleRemoteEvent(ENetPeer* sender, PacketReader& reader);

        // Property serialization
        void WritePropertyValue(PacketWriter& writer, const PropertyValue& value);
        PropertyValue ReadPropertyValue(PacketReader& reader);
    };
}
