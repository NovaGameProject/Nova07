// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/NetworkService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Objects/InstanceFactory.hpp"
#include "Common/Log.hpp"

namespace Nova {

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

        uint16_t propCount = reader.ReadU16();
        auto* desc = ClassDescriptor::Get(className);
        for (uint16_t i = 0; i < propCount; i++) {
            std::string propName = reader.ReadString();
            PropertyValue value = ReadPropertyValue(reader);
            if (desc) {
                auto* accessor = desc->FindProperty(propName);
                if (accessor) {
                    accessor->set(instance.get(), value);
                }
            }
        }
    }

    void NetworkService::HandleDestroyObject(ENetPeer* sender, PacketReader& reader) {
        NetworkID networkID = reader.ReadU32();

        auto it = mClientInstances.find(networkID);
        if (it != mClientInstances.end()) {
            it->second->Destroy();
            mClientInstances.erase(it);

            if (auto dm = GetDataModel()) {
                if (auto ws = dm->GetService<Workspace>()) {
                    ws->RefreshCachedParts();
                }
            }
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
            if (propertyName == "CFrame" && value.isCFrame()) {
                if (auto* bp = dynamic_cast<BasePart*>(instance.get())) {
                    bp->SetNetworkTargetCFrame(value.toCFrame());
                    return;
                }
            }
            accessor->set(instance.get(), value);
        }
    }

    void NetworkService::HandleBatchPropertyUpdate(ENetPeer* sender, PacketReader& reader) {
        NetworkID networkID = reader.ReadU32();
        uint16_t count = reader.ReadU16();

        auto it = mClientInstances.find(networkID);
        if (it == mClientInstances.end()) {
            for (uint16_t i = 0; i < count; i++) {
                reader.ReadString();
                ReadPropertyValue(reader);
            }
            LOG_WRN("Network", "BatchUpdate: unknown instance ID=%u count=%u (skipped)", networkID, count);
            return;
        }

        auto instance = it->second;
        if (!instance) {
            mClientInstances.erase(it);
            return;
        }

        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (!desc) return;

        for (uint16_t i = 0; i < count; i++) {
            std::string propertyName = reader.ReadString();
            PropertyValue value = ReadPropertyValue(reader);
            if (propertyName == "CFrame" && value.isCFrame()) {
                if (auto* bp = dynamic_cast<BasePart*>(instance.get())) {
                    bp->SetNetworkTargetCFrame(value.toCFrame());
                    continue;
                }
            }
            auto* accessor = desc->FindProperty(propertyName);
            if (accessor) {
                accessor->set(instance.get(), value);
            }
        }
    }

    void NetworkService::HandleRemoteEvent(ENetPeer* sender, PacketReader& reader) {
        // TODO: Fire remote event
    }

    void NetworkService::HandleBulkCFrameUpdate(ENetPeer* sender, PacketReader& reader) {
        uint16_t count = reader.ReadU16();
        for (uint16_t i = 0; i < count; i++) {
            NetworkID networkID = reader.ReadU32();
            CFrame cf;
            cf.position = reader.ReadVec3();
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    cf.rotation[r][c] = reader.ReadFloat();
                }
            }

            auto it = mClientInstances.find(networkID);
            if (it == mClientInstances.end()) continue;

            if (auto* bp = dynamic_cast<BasePart*>(it->second.get())) {
                bp->SetNetworkTargetCFrame(cf);
            }
        }
    }

    void NetworkService::WritePropertyValue(PacketWriter& writer, const PropertyValue& value) {
        writer.WriteU8(static_cast<uint8_t>(value.kind));

        switch (value.kind) {
            case PropertyValue::Kind::Nil:
                break;
            case PropertyValue::Kind::Bool:
                writer.WriteU8(value.toBool() ? 1 : 0);
                break;
            case PropertyValue::Kind::Int: {
                int64_t v = value.toInt();
                writer.WriteU32(static_cast<uint32_t>(static_cast<int32_t>(v)));
                break;
            }
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
                return PropertyValue(static_cast<int64_t>(static_cast<int32_t>(reader.ReadU32())));
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

}
