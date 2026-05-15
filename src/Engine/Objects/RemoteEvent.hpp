// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/Objects/Instance.hpp"
#include "Engine/Common/Signal.hpp"
#include <string>
#include <vector>

namespace Nova {
    class Player;

    class RemoteEvent : public Instance {
    public:
        Signal OnServerEvent;   // Fired on server: (Player, args...)
        Signal OnClientEvent;   // Fired on client: (args...)

        RemoteEvent() : Instance("RemoteEvent") {}

        std::string GetClassName() const override { return "RemoteEvent"; }
        std::string GetName() const override { return m_debugName; }

        // Client calls this to send to server
        void FireServer();

        // Server calls these to send to clients
        void FireClient(std::shared_ptr<Player> player);
        void FireAllClients();
    };
}
