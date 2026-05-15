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
#include <cstdint>
#include <memory>

namespace Nova {
    class Model;
    class Humanoid;

    class Player : public Instance {
    public:
        std::string playerName;
        uint32_t playerID = 0;

        Signal CharacterAdded;
        Signal CharacterRemoving;

        Player() : Instance("Player") {}
        Player(const std::string& name, uint32_t id)
            : Instance(name), playerName(name), playerID(id) {}

        std::string GetClassName() const override { return "Player"; }
        std::string GetName() const override { return playerName; }

        std::shared_ptr<Model> GetCharacter() const { return character.lock(); }
        void SetCharacter(std::shared_ptr<Model> model) { character = model; }

        void Kick(const std::string& reason);

    private:
        std::weak_ptr<Model> character;
    };
}
