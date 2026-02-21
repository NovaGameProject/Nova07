// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Script.hpp"

namespace Nova {
    namespace Props {
        struct LocalScriptProps {
            rfl::Flatten<ScriptProps> base;
        };
    }

    class LocalScript : public Script {
    public:
        Props::LocalScriptProps props;
        NOVA_OBJECT(LocalScript, props)

        LocalScript() : Script("LocalScript") {}
        ~LocalScript() override = default;

        void Run() override;
    };
}
