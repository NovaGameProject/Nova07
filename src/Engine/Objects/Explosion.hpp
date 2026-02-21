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
#include "Common/MathTypes.hpp"

namespace Nova {
    namespace Props {
        struct ExplosionProps {
            rfl::Flatten<InstanceProps> base;
            rfl::Rename<"Position", Vector3Reflect> Position;
            float BlastRadius = 4.0f;
            float BlastPressure = 500000.0f;
        };
    }

    class Explosion : public Instance {
    public:
        Props::ExplosionProps props;
        NOVA_OBJECT(Explosion, props)
        
        Signal Hit; // Fires for each affected part
        
        // Visual effect state
        bool m_visualActive = false;
        float m_visualTime = 0.0f;
        float m_visualDuration = 0.5f;
        
        void UpdateVisual(float dt);
        bool IsVisualActive() const { return m_visualActive; }
        float GetVisualProgress() const { return m_visualActive ? (m_visualTime / m_visualDuration) : 0.0f; }
        
        Explosion();

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;
    };
}
