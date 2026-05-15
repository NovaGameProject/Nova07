// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/Networking/NetworkID.hpp"
#include "Common/MathTypes.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <variant>

namespace Nova {

    enum class PacketType : uint8_t {
        CreateObject = 1,
        DestroyObject = 2,
        PropertyUpdate = 3,
        FullSync = 4,
        RemoteEvent = 5,
        RemoteFunction = 6,
        RemoteFunctionResponse = 7,
        PlayerJoin = 8,
        PlayerLeave = 9,
    };

    struct PacketHeader {
        PacketType type;
        uint16_t sequence;
    };

    struct CreateObjectPacket {
        NetworkID networkID;
        NetworkID parentNetworkID;
        std::string className;
        // Initial properties serialized separately
    };

    struct DestroyObjectPacket {
        NetworkID networkID;
    };

    struct PropertyUpdatePacket {
        NetworkID networkID;
        std::string propertyName;
        // Value serialized based on type
    };

    struct RemoteEventPacket {
        NetworkID targetID;
        std::string eventName;
        // Arguments serialized separately
    };

    struct RemoteFunctionPacket {
        NetworkID targetID;
        uint32_t callID;
        std::string functionName;
        // Arguments serialized separately
    };

    struct PlayerJoinPacket {
        NetworkID playerID;
        std::string playerName;
    };

    struct PlayerLeavePacket {
        NetworkID playerID;
    };

    // Simple binary serialization helpers
    class PacketWriter {
    public:
        PacketWriter() = default;

        void WriteU8(uint8_t v) { data.push_back(v); }
        void WriteU16(uint16_t v) {
            data.push_back((v >> 8) & 0xFF);
            data.push_back(v & 0xFF);
        }
        void WriteU32(uint32_t v) {
            data.push_back((v >> 24) & 0xFF);
            data.push_back((v >> 16) & 0xFF);
            data.push_back((v >> 8) & 0xFF);
            data.push_back(v & 0xFF);
        }
        void WriteFloat(float v) {
            uint32_t bits;
            memcpy(&bits, &v, sizeof(float));
            WriteU32(bits);
        }
        void WriteString(const std::string& s) {
            WriteU16(static_cast<uint16_t>(s.size()));
            data.insert(data.end(), s.begin(), s.end());
        }
        void WriteVec3(const Vector3& v) {
            WriteFloat(v.x);
            WriteFloat(v.y);
            WriteFloat(v.z);
        }
        void WriteBytes(const uint8_t* ptr, size_t len) {
            data.insert(data.end(), ptr, ptr + len);
        }

        const std::vector<uint8_t>& GetData() const { return data; }
        size_t Size() const { return data.size(); }

    private:
        std::vector<uint8_t> data;
    };

    class PacketReader {
    public:
        PacketReader(const uint8_t* data, size_t len)
            : data(data), len(len), pos(0) {}

        uint8_t ReadU8() {
            if (pos >= len) return 0;
            return data[pos++];
        }
        uint16_t ReadU16() {
            if (pos + 2 > len) return 0;
            uint16_t v = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
            pos += 2;
            return v;
        }
        uint32_t ReadU32() {
            if (pos + 4 > len) return 0;
            uint32_t v = (static_cast<uint32_t>(data[pos]) << 24) |
                         (static_cast<uint32_t>(data[pos + 1]) << 16) |
                         (static_cast<uint32_t>(data[pos + 2]) << 8) |
                          static_cast<uint32_t>(data[pos + 3]);
            pos += 4;
            return v;
        }
        float ReadFloat() {
            uint32_t bits = ReadU32();
            float v;
            memcpy(&v, &bits, sizeof(float));
            return v;
        }
        std::string ReadString() {
            uint16_t len = ReadU16();
            if (pos + len > this->len) return "";
            std::string s(reinterpret_cast<const char*>(data + pos), len);
            pos += len;
            return s;
        }
        Vector3 ReadVec3() {
            return { ReadFloat(), ReadFloat(), ReadFloat() };
        }

        bool HasMore() const { return pos < len; }
        size_t GetPosition() const { return pos; }

    private:
        const uint8_t* data;
        size_t len;
        size_t pos;
    };

}
