// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Explosion.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Common/Log.hpp"
#include <glm/glm.hpp>

namespace Nova {
    Explosion::Explosion() : Instance("Explosion") {
        LOG_DBG("Explosion", "Created");
    }

    void Explosion::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);

        LOG_DBG("Explosion", "OnAncestorChanged, parent=%s", newParent ? newParent->GetName().c_str() : "nil");

        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                if (auto physics = dm->GetService<PhysicsService>()) {
                    physics->QueueExplosion(position, BlastRadius, BlastPressure);

                    m_visualActive = true;
                    m_visualTime = 0.0f;
                }
            }
        }
    }

    void Explosion::UpdateVisual(float dt) {
        if (!m_visualActive) return;

        m_visualTime += dt;
        if (m_visualTime >= m_visualDuration) {
            m_visualActive = false;
            // Remove self from parent after effect completes
            SetParent(nullptr);
        }
    }
}
