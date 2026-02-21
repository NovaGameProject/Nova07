// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include <string>

namespace Nova {
    namespace Props {
        struct ScriptProps {
            rfl::Flatten<InstanceProps> base;
            std::string Source;
            bool Disabled = false;
        };
    }

    class Script : public Instance {
    public:
        Props::ScriptProps props;
        NOVA_OBJECT(Script, props)

        Script(std::string name = "Script");

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;

        virtual void Run();

    private:
        bool m_hasRun = false;
    };
}
