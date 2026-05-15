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
    class LocalScript : public Script {
    public:
        LocalScript() : Script("LocalScript") {}
        ~LocalScript() override = default;

        void Run() override;
        std::string GetClassName() const override { return "LocalScript"; }
    };
}
