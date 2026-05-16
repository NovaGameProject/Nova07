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
#include <chrono>

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

    // ===== Packet Compression =====
    std::vector<uint8_t> NetworkService::CompressPacket(const uint8_t* data, size_t len) {
        size_t maxCompressed = ZSTD_compressBound(len);
        std::vector<uint8_t> compressed(1 + maxCompressed);
        compressed[0] = 0x01; // compression flag
        size_t compressedSize = ZSTD_compress(compressed.data() + 1, maxCompressed, data, len, 3);
        if (ZSTD_isError(compressedSize)) {
            // Compression failed — return empty (caller should send uncompressed)
            return {};
        }
        compressed.resize(1 + compressedSize);
        return compressed;
    }

    std::vector<uint8_t> NetworkService::DecompressPacket(const uint8_t* data, size_t len) {
        if (len < 1 || data[0] != 0x01) return {}; // not compressed
        size_t decompressedSize = ZSTD_getFrameContentSize(data + 1, len - 1);
        if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN || decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
            return {};
        }
        std::vector<uint8_t> decompressed(decompressedSize);
        size_t result = ZSTD_decompress(decompressed.data(), decompressedSize, data + 1, len - 1);
        if (ZSTD_isError(result)) {
            return {};
        }
        return decompressed;
    }

    // ===== Network Thread =====
    void NetworkService::NetworkThreadFunc() {
        while (!mStopping) {
            if (mHost) {
                ENetEvent event;
                while (enet_host_service(mHost, &event, 0) > 0) {
                    switch (event.type) {
                        case ENET_EVENT_TYPE_CONNECT: {
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;
                            pkt.data = {0x01};
                            mIncomingPackets.push(std::move(pkt));
                            break;
                        }
                        case ENET_EVENT_TYPE_RECEIVE: {
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;

                            // Decompress if compressed
                            if (event.packet->dataLength >= 2 && event.packet->data[0] == 0x01) {
                                auto decompressed = DecompressPacket(event.packet->data, event.packet->dataLength);
                                if (!decompressed.empty()) {
                                    pkt.data = std::move(decompressed);
                                } else {
                                    pkt.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                                }
                            } else {
                                pkt.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                            }

                            mIncomingPackets.push(std::move(pkt));
                            enet_packet_destroy(event.packet);
                            break;
                        }
                        case ENET_EVENT_TYPE_DISCONNECT: {
                            std::lock_guard<std::mutex> lock(mIncomingMutex);
                            IncomingPacket pkt;
                            pkt.sender = event.peer;
                            pkt.data = {0x02};
                            mIncomingPackets.push(std::move(pkt));
                            break;
                        }
                        default:
                            break;
                    }
                }

                static constexpr size_t BATCH_SIZE = 64;
                while (true) {
                    std::vector<OutgoingPacket> batch;
                    batch.reserve(BATCH_SIZE);
                    {
                        std::lock_guard<std::mutex> lock(mOutgoingMutex);
                        for (size_t i = 0; i < BATCH_SIZE && !mOutgoingPackets.empty(); i++) {
                            batch.push_back(std::move(mOutgoingPackets.front()));
                            mOutgoingPackets.pop();
                        }
                    }
                    if (batch.empty()) break;

                    for (auto& out : batch) {
                        uint32_t flags = out.reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
                        const uint8_t* sendData = out.data.data();
                        size_t sendLen = out.data.size();

                        // Compress large packets
                        std::vector<uint8_t> compressedBuf;
                        if (sendLen >= COMPRESS_THRESHOLD) {
                            compressedBuf = CompressPacket(sendData, sendLen);
                            if (!compressedBuf.empty() && compressedBuf.size() < sendLen) {
                                sendData = compressedBuf.data();
                                sendLen = compressedBuf.size();
                            }
                        }

                        ENetPacket* packet = enet_packet_create(sendData, sendLen, flags);
                        enet_peer_send(out.target, 0, packet);
                    }
                }

                enet_host_flush(mHost);
            }

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
        mLastSentCFrame.clear();

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

        ProcessIncomingPackets();

        if (mIsServer) {
            ProcessSyncQueue();

            mSendTimer += dt;
            if (mSendTimer >= SEND_RATE) {
                mSendTimer = 0.0f;
                DrainPendingProperties();
            }

            mDriftCheckTimer += dt;
            if (mDriftCheckTimer >= DRIFT_CHECK_INTERVAL) {
                mDriftCheckTimer = 0.0f;
                CheckForDrift();
            }

            mDiagTimer += dt;
            if (mDiagTimer >= 2.0f) {
                mDiagTimer = 0.0f;
                size_t queueSize;
                {
                    std::lock_guard<std::mutex> lock(mOutgoingMutex);
                    queueSize = mOutgoingPackets.size();
                }
                size_t dropped = mDroppedPackets.exchange(0);
                size_t syncingCount = 0;
                for (auto& sync : mPendingSyncs) {
                    if (sync.isSyncing) syncingCount++;
                }
                LOG_INF("Network", "Diag: outQueue=%zu pending=%zu dropped=%zu syncing=%zu lastSent=%zu markDirty=%zu",
                    queueSize, mPendingProperties.size(), dropped, syncingCount,
                    mLastSentCFrame.size(), mMarkDirtyCalls.exchange(0));
            }
        }

        if (mIsClient) {
            // Interpolate all replicated BaseParts toward their network targets
            for (auto& [id, instance] : mClientInstances) {
                if (auto* bp = dynamic_cast<BasePart*>(instance.get())) {
                    bp->UpdateNetworkInterpolation(dt);
                }
            }

            mDiagTimer += dt;
            if (mDiagTimer >= 2.0f) {
                mDiagTimer = 0.0f;
                size_t inQueueSize;
                {
                    std::lock_guard<std::mutex> lock(mIncomingMutex);
                    inQueueSize = mIncomingPackets.size();
                }
                LOG_INF("Network", "ClientDiag: instances=%zu incomingQueue=%zu",
                    mClientInstances.size(), inQueueSize);
            }
        }
    }

    void NetworkService::ProcessIncomingPackets() {
        std::queue<IncomingPacket> packets;
        {
            std::lock_guard<std::mutex> lock(mIncomingMutex);
            packets.swap(mIncomingPackets);
        }

        static constexpr size_t MAX_PACKETS_PER_FRAME = 200;
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
                    case PacketType::BatchPropertyUpdate:
                        HandleBatchPropertyUpdate(pkt.sender, reader);
                        break;
                    case PacketType::BulkCFrameUpdate:
                        HandleBulkCFrameUpdate(pkt.sender, reader);
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

        if (!packets.empty()) {
            std::lock_guard<std::mutex> lock(mIncomingMutex);
            while (!packets.empty()) {
                mIncomingPackets.push(std::move(packets.front()));
                packets.pop();
            }
        }

        if (needsRefresh) {
            if (auto dm = GetDataModel()) {
                if (auto ws = dm->GetService<Workspace>()) {
                    ws->RefreshCachedParts();
                }
            }
        }
    }

    void NetworkService::ProcessSyncQueue() {
        static constexpr size_t SYNC_PER_TICK = 50;

        for (auto it = mPendingSyncs.begin(); it != mPendingSyncs.end();) {
            auto& sync = *it;
            size_t sent = 0;

            while (sync.nextIndex < sync.objects.size() && sent < SYNC_PER_TICK) {
                auto obj = sync.objects[sync.nextIndex].lock();
                if (obj && obj->networkID != 0 && !obj->IsDestroyed()) {
                    SendCreateObject(sync.peer, obj.get());
                    sync.syncedIDs.insert(obj->networkID);
                }
                sync.nextIndex++;
                sent++;
            }

            if (sync.nextIndex >= sync.objects.size()) {
                sync.isSyncing = false;
                size_t flushed = 0;
                std::unordered_set<NetworkID> sentCreates;
                for (auto& change : sync.queuedChanges) {
                    if (sync.syncedIDs.contains(change.targetID)) continue;

                    auto* instance = mIDRegistry.GetInstance(change.targetID);
                    if (!instance || instance->IsDestroyed()) continue;

                    // Instance wasn't in the original FullSync — send CreateObject first
                    if (!sentCreates.contains(change.targetID)) {
                        SendCreateObject(sync.peer, instance);
                        sentCreates.insert(change.targetID);
                    }

                    auto* desc = ClassDescriptor::Get(instance->GetClassName());
                    if (!desc) continue;

                    auto* accessor = desc->FindProperty(change.propertyName);
                    if (!accessor) continue;

                    PropertyValue value = accessor->get(instance);
                    SendPropertyUpdate(sync.peer, change.targetID, change.propertyName, value);
                    flushed++;
                }

                LOG_INF("Network", "FullSync complete (%zu queued changes flushed)", flushed);
                it = mPendingSyncs.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ===== Connection Handling =====
    void NetworkService::HandleConnect(ENetPeer* peer) {
        if (!mIsServer) {
            LOG_INF("Network", "Connected to server");
            return;
        }

        auto player = std::make_shared<Player>("Player_" + std::to_string(peer->connectID),
                                                peer->connectID);
        mPlayers.push_back(player);
        mPeerToPlayer[peer] = player;

        auto dm = GetDataModel();
        if (dm) {
            player->SetParent(dm);
        }

        LOG_INF("Network", "Player connected: %s (ID: %u)", player->GetName().c_str(), peer->connectID);

        PacketWriter writer;
        writer.WriteU8(static_cast<uint8_t>(PacketType::PlayerJoin));
        writer.WriteU32(player->playerID);
        writer.WriteString(player->playerName);
        QueueSend(peer, writer, PacketType::PlayerJoin);

        SendFullSync(peer);
    }

    void NetworkService::HandleDisconnect(ENetPeer* peer) {
        auto it = mPeerToPlayer.find(peer);
        if (it != mPeerToPlayer.end()) {
            auto player = it->second;
            LOG_INF("Network", "Player disconnected: %s", player->GetName().c_str());

            mPlayers.erase(std::remove(mPlayers.begin(), mPlayers.end(), player), mPlayers.end());
            mPeerToPlayer.erase(it);

            PacketWriter writer;
            writer.WriteU8(static_cast<uint8_t>(PacketType::PlayerLeave));
            writer.WriteU32(player->playerID);
            for (auto& [otherPeer, otherPlayer] : mPeerToPlayer) {
                if (otherPeer != peer) {
                    QueueSend(otherPeer, writer, PacketType::PlayerLeave);
                }
            }

            player->Destroy();
        }
    }

}
