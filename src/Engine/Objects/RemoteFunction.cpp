// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/RemoteFunction.hpp"
#include "Common/Log.hpp"

namespace Nova {

    void RemoteFunction::InvokeServer() {
        // TODO: Send to server and wait for response
        LOG_DBG("RemoteFunction", "InvokeServer called on '%s'", m_debugName.c_str());
    }

    void RemoteFunction::InvokeClient(std::shared_ptr<Player> player) {
        // TODO: Send to client and wait for response
        LOG_DBG("RemoteFunction", "InvokeClient called on '%s'", m_debugName.c_str());
    }

}
