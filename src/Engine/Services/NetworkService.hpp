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
#include <zstd.h>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
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

        // Broadcast destroy to all clients
        void BroadcastDestroyObject(NetworkID id);

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

        // Dirty property tracking
        struct DirtyProperty {
            NetworkID targetID;
            std::string propertyName;
            std::shared_ptr<Instance> instance;  // Keep instance alive
        };

        // Incremental sync queue (server side)
        struct PendingSync {
            ENetPeer* peer;
            std::vector<std::weak_ptr<Instance>> objects;
            size_t nextIndex = 0;
            bool isSyncing = false;  // True while FullSync is in progress
            std::vector<DirtyProperty> queuedChanges;  // Changes during sync
            std::unordered_set<NetworkID> syncedIDs;  // IDs already sent in FullSync
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
            PacketType type;  // For backpressure decisions
        };
        std::queue<OutgoingPacket> mOutgoingPackets;
        std::mutex mOutgoingMutex;
        static constexpr size_t MAX_OUTGOING_QUEUE = 4096;
        std::atomic<size_t> mDroppedPackets{0};

        // Pending property updates (deduplicated by instance+property, only main thread)
        std::unordered_map<uint64_t, DirtyProperty> mPendingProperties;

        // CFrame drift detection: track last replicated CFrame per object
        std::unordered_map<NetworkID, CFrame> mLastSentCFrame;

        // Send rate control
        float mSendTimer = 0.0f;
        static constexpr float SEND_RATE = 1.0f / 60.0f;  // 60 Hz
        void DrainPendingProperties();

        // Periodic drift check for sleeping bodies
        float mDriftCheckTimer = 0.0f;
        static constexpr float DRIFT_CHECK_INTERVAL = 0.5f;  // Check every 500ms
        void CheckForDrift();

        // Diagnostics
        float mDiagTimer = 0.0f;
        std::atomic<size_t> mMarkDirtyCalls{0};

        // Network thread entry point
        void NetworkThreadFunc();

        // Server: assign NetworkIDs to instance tree
        void AssignNetworkIDs(Instance* root);

        // Server: send full world state to a client
        void SendFullSync(ENetPeer* peer);

        // Server: send only replicated properties for an instance
        void SendReplicatedProperties(ENetPeer* peer, Instance* instance, NetworkID id);

        // Helpers
        void ProcessIncomingPackets();
        void ProcessSyncQueue();

        // Packet sending (queues to network thread)
        void QueueSend(ENetPeer* peer, const PacketWriter& writer, PacketType type, bool reliable = true);
        void SendCreateObject(ENetPeer* peer, Instance* instance);
        void SendDestroyObject(ENetPeer* peer, NetworkID id);
        void SendPropertyUpdate(ENetPeer* peer, NetworkID id, const std::string& prop, const PropertyValue& value);
        void SendBatchProperties(ENetPeer* peer, NetworkID id, const std::vector<std::pair<std::string, PropertyValue>>& properties);
        void SendBulkCFrameUpdate(ENetPeer* peer, const std::vector<std::pair<NetworkID, CFrame>>& updates);

        // Packet handlers (called from main thread)
        void HandleConnect(ENetPeer* peer);
        void HandleDisconnect(ENetPeer* peer);
        void HandleCreateObject(ENetPeer* sender, PacketReader& reader);
        void HandleDestroyObject(ENetPeer* sender, PacketReader& reader);
        void HandlePropertyUpdate(ENetPeer* sender, PacketReader& reader);
        void HandleBatchPropertyUpdate(ENetPeer* sender, PacketReader& reader);
        void HandleBulkCFrameUpdate(ENetPeer* sender, PacketReader& reader);
        void HandleRemoteEvent(ENetPeer* sender, PacketReader& reader);

        // Property serialization
        void WritePropertyValue(PacketWriter& writer, const PropertyValue& value);
        PropertyValue ReadPropertyValue(PacketReader& reader);

        // Packet compression (zstd)
        static constexpr size_t COMPRESS_THRESHOLD = 128;
        std::vector<uint8_t> CompressPacket(const uint8_t* data, size_t len);
        std::vector<uint8_t> DecompressPacket(const uint8_t* data, size_t len);
    };
}
