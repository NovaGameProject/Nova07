// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Player.hpp"
#include "Common/Log.hpp"

namespace Nova {

    void Player::Kick(const std::string& reason) {
        LOG_INF("Player", "Player '%s' kicked: %s", playerName.c_str(), reason.c_str());
        // TODO: Disconnect from network service
        Destroy();
    }

}
