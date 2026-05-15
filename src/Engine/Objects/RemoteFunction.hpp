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

namespace Nova {
    class Player;

    class RemoteFunction : public Instance {
    public:
        // Callbacks (set by Lua)
        // OnServerInvoke: function(player, args...) -> return values
        // OnClientInvoke: function(args...) -> return values

        RemoteFunction() : Instance("RemoteFunction") {}

        std::string GetClassName() const override { return "RemoteFunction"; }
        std::string GetName() const override { return m_debugName; }

        // Client calls this to invoke on server (blocking)
        void InvokeServer();

        // Server calls this to invoke on specific client (blocking)
        void InvokeClient(std::shared_ptr<Player> player);
    };
}
