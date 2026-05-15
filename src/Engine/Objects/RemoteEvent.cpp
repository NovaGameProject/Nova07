// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/RemoteEvent.hpp"
#include "Common/Log.hpp"

namespace Nova {

    void RemoteEvent::FireServer() {
        // TODO: Send to server via NetworkService
        LOG_DBG("RemoteEvent", "FireServer called on '%s'", m_debugName.c_str());
    }

    void RemoteEvent::FireClient(std::shared_ptr<Player> player) {
        // TODO: Send to specific client via NetworkService
        LOG_DBG("RemoteEvent", "FireClient called on '%s'", m_debugName.c_str());
    }

    void RemoteEvent::FireAllClients() {
        // TODO: Broadcast to all clients via NetworkService
        LOG_DBG("RemoteEvent", "FireAllClients called on '%s'", m_debugName.c_str());
    }

}
