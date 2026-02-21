// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <iostream>

namespace Nova {

    void PhysicsService::ProcessExplosions() {
        std::vector<ExplosionRequest> explosions;
        {
            std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
            explosions.swap(mPendingExplosions);
        }
        if (explosions.empty()) return;

        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();
        for (auto& exp : explosions) {
            std::unordered_set<BasePart*> affectedParts;
            std::unordered_map<BasePart*, glm::vec3> partImpulses;

            {
                std::shared_lock<std::shared_mutex> mapLock(mMapsMutex);
                for (auto& [id, assembly] : mBodyToAssembly) {
                    for (auto& wp : assembly->parts) {
                        auto p = wp.lock();
                        if (!p) continue;

                        auto itRel = assembly->relativeTransforms.find(p.get());
                        if (itRel == assembly->relativeTransforms.end()) continue;

                        CFrame worldCF = assembly->rootPart->basePartProps->CFrame.get().to_nova() * itRel->second;
                        float distance = glm::length(worldCF.position - exp.position);
                        if (distance <= exp.radius) {
                            affectedParts.insert(p.get());
                            
                            glm::vec3 direction = (distance > 0.01f) ? glm::normalize(worldCF.position - exp.position) : glm::vec3(0, 1, 0);
                            float impulseMagnitude = exp.pressure * (1.0f - distance / exp.radius) * 5.0f; 
                            partImpulses[p.get()] = direction * impulseMagnitude;
                        }
                    }
                }
            }

            for (auto* part : affectedParts) BreakJoints(part);

            UpdateAssemblies();

            std::shared_lock<std::shared_mutex> mapLock(mMapsMutex);
            std::unordered_set<JPH::BodyID, BodyIDHasher> updatedBodies;
            for (auto& [p, impulse] : partImpulses) {
                if (p->physicsBodyID.IsInvalid()) continue;
                if (bi.GetMotionType(p->physicsBodyID) == JPH::EMotionType::Static) continue;
                
                auto itAss = mPartToAssembly.find(p);
                if (itAss == mPartToAssembly.end()) continue;
                
                auto itRel = itAss->second->relativeTransforms.find(p);
                if (itRel == itAss->second->relativeTransforms.end()) continue;

                CFrame worldCF = itAss->second->rootPart->basePartProps->CFrame.get().to_nova() * itRel->second;
                bi.AddImpulse(p->physicsBodyID, JPH::Vec3(impulse.x, impulse.y, impulse.z), 
                    JPH::RVec3(worldCF.position.x, worldCF.position.y, worldCF.position.z));
                
                updatedBodies.insert(p->physicsBodyID);
            }
            
            for (auto id : updatedBodies) {
                bi.ActivateBody(id);
            }
        }
    }

    std::vector<std::pair<std::shared_ptr<BasePart>, float>> PhysicsService::ApplyExplosionImpulse(
        glm::vec3 position, float radius, float pressure) {

        std::vector<std::pair<std::shared_ptr<BasePart>, float>> affectedParts;
        std::lock_guard<std::recursive_mutex> lock(mPhysicsMutex);
        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();

        std::shared_lock<std::shared_mutex> mapLock(mMapsMutex);
        for (auto& [id, assembly] : mBodyToAssembly) {
            bool bodyAffected = false;
            
            for (auto& wp : assembly->parts) {
                auto p = wp.lock();
                if (!p) continue;

                auto itRel = assembly->relativeTransforms.find(p.get());
                if (itRel == assembly->relativeTransforms.end()) continue;

                CFrame worldCF = assembly->rootPart->basePartProps->CFrame.get().to_nova() * itRel->second;
                float distance = glm::length(worldCF.position - position);
                
                if (distance <= radius) {
                    bodyAffected = true;
                    affectedParts.push_back({p, distance});

                    if (bi.GetMotionType(id) != JPH::EMotionType::Static) {
                        glm::vec3 direction = (distance > 0.01f) ? glm::normalize(worldCF.position - position) : glm::vec3(0, 1, 0);
                        float impulseMagnitude = pressure * (1.0f - distance / radius) * 5.0f;
                        glm::vec3 impulse = direction * impulseMagnitude;
                        
                        bi.AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z),
                            JPH::RVec3(worldCF.position.x, worldCF.position.y, worldCF.position.z));
                    }
                }
            }

            if (bodyAffected && bi.GetMotionType(id) != JPH::EMotionType::Static) {
                bi.ActivateBody(id);
            }
        }

        return affectedParts;
    }
}
