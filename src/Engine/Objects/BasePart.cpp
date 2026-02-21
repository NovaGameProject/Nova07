// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/JointInstance.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace Nova {
    BasePart::~BasePart() {
        if (!physicsBodyID.IsInvalid()) {
            if (auto physics = registeredService.lock()) {
                physics->UnregisterPart(this);
            } else if (auto dm = GetDataModel()) {
                if (auto physics = dm->GetService<PhysicsService>()) {
                    physics->UnregisterPart(this);
                }
            }
        }
    }

    void BasePart::InitializePhysics() {
        if (!basePartProps) return;
        auto cf = basePartProps->CFrame.get().to_nova();
        // Since we removed interpolation state from header, we don't need to sync prev/curr anymore
    }

    void BasePart::BreakJoints() {
        std::vector<std::shared_ptr<Instance>> toRemove;
        for (auto& child : children) {
            if (std::dynamic_pointer_cast<JointInstance>(child)) {
                toRemove.push_back(child);
            }
        }
        for (auto& joint : toRemove) {
            joint->SetParent(nullptr);
        }
    }

    void BasePart::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);

        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                auto physics = dm->GetService<PhysicsService>();
                if (physicsBodyID.IsInvalid()) {
                    physics->BulkRegisterParts({ std::static_pointer_cast<BasePart>(shared_from_this()) });
                    if (!physics->IsDeferring()) InitializePhysics();
                }
            } else {
                if (!physicsBodyID.IsInvalid()) {
                    if (auto physics = registeredService.lock()) {
                        physics->UnregisterPart(this);
                    } else if (auto p = dm->GetService<PhysicsService>()) {
                        p->UnregisterPart(this);
                    }
                }
            }
        } else {
            // Detached from tree
            if (!physicsBodyID.IsInvalid()) {
                if (auto physics = registeredService.lock()) {
                    physics->UnregisterPart(this);
                }
            }
        }
    }

    void BasePart::OnPropertyChanged(const std::string& name) {
        if (physicsBodyID.IsInvalid()) return;

        auto physics = registeredService.lock();
        if (!physics) {
            auto dm = GetDataModel();
            if (dm) physics = dm->GetService<PhysicsService>();
        }
        
        if (!physics) return;

        std::lock_guard<std::recursive_mutex> physicsLock(physics->GetPhysicsMutex());
        JPH::BodyInterface &bi = physics->GetPhysicsSystem()->GetBodyInterface();

        if (name == "CFrame") {
            auto cf = basePartProps->CFrame.get().to_nova();
            
            // If part is in an assembly, we need to move the assembly root such that this part ends up at cf
            CFrame bodyCF = cf;
            {
                std::shared_lock<std::shared_mutex> mapLock(physics->mMapsMutex);
                auto it = physics->mPartToAssembly.find(this);
                if (it != physics->mPartToAssembly.end()) {
                    auto assembly = it->second;
                    auto itRel = assembly->relativeTransforms.find(this);
                    if (itRel != assembly->relativeTransforms.end()) {
                        bodyCF = cf * itRel->second.inverse();
                    }
                }
            }

            glm::quat q = glm::normalize(glm::quat_cast(bodyCF.rotation));
            if (glm::any(glm::isnan(q))) q = glm::quat(1, 0, 0, 0);

            bi.SetPositionAndRotation(physicsBodyID, 
                JPH::RVec3(bodyCF.position.x, bodyCF.position.y, bodyCF.position.z),
                JPH::Quat(q.x, q.y, q.z, q.w),
                JPH::EActivation::Activate);
        }
        else if (name == "Anchored") {
            // Trigger assembly update to potentially merge/split/convert
            physics->RequestAssemblyUpdate(this);
        }
        else if (name == "Size") {
            physics->UnregisterPart(this);
            physics->BulkRegisterParts({ std::static_pointer_cast<BasePart>(shared_from_this()) });
        }
    }

    glm::vec3 BasePart::GetVelocity() {
        if (physicsBodyID.IsInvalid()) return glm::vec3(0.0f);
        
        auto physics = registeredService.lock();
        if (!physics) return glm::vec3(0.0f);
        
        auto* physicsSystem = physics->GetPhysicsSystem();
        if (!physicsSystem) return glm::vec3(0.0f);
        
        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();
        JPH::Vec3 vel = bi.GetLinearVelocity(physicsBodyID);
        
        return glm::vec3(vel.GetX(), vel.GetY(), vel.GetZ());
    }

    void BasePart::SetVelocity(const glm::vec3& velocity) {
        if (physicsBodyID.IsInvalid()) return;
        
        auto physics = registeredService.lock();
        if (!physics) return;
        
        auto* physicsSystem = physics->GetPhysicsSystem();
        if (!physicsSystem) return;
        
        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();
        bi.SetLinearVelocity(physicsBodyID, JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }
}
