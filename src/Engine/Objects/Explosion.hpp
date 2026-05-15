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
    class Explosion : public Instance {
    public:
        Vector3 position;
        float BlastRadius = 4.0f;
        float BlastPressure = 500000.0f;

        Signal Hit;

        bool m_visualActive = false;
        float m_visualTime = 0.0f;
        float m_visualDuration = 0.5f;

        void UpdateVisual(float dt);
        bool IsVisualActive() const { return m_visualActive; }
        float GetVisualProgress() const { return m_visualActive ? (m_visualTime / m_visualDuration) : 0.0f; }

        Explosion();

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;

        std::string GetClassName() const override { return "Explosion"; }
        std::string GetName() const override { return m_debugName; }
    };
}
