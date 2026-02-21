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
#include <glm/glm.hpp>
#include <iostream>

namespace Nova {
    Explosion::Explosion() : Instance("Explosion") {}

    void Explosion::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);
        
        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                if (auto physics = dm->GetService<PhysicsService>()) {
                    glm::vec3 pos = props.Position.get().to_glm();
                    float radius = props.BlastRadius;
                    
                    std::cout << "[Explosion] Queuing explosion at (" << pos.x << ", " << pos.y << ", " << pos.z 
                              << ") with radius " << radius << std::endl;
                    
                    // Queue the explosion for processing on the physics thread
                    // This prevents race conditions with the physics simulation
                    physics->QueueExplosion(pos, radius, props.BlastPressure);
                    
                    // Start visual effect
                    m_visualActive = true;
                    m_visualTime = 0.0f;
                    
                    // Note: Hit signal will be fired by PhysicsService when processing the explosion
                    // This is handled internally by the physics thread
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
