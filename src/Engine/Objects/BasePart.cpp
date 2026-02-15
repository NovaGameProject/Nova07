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
                physics->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
            } else if (auto dm = GetDataModel()) {
                if (auto physics = dm->GetService<PhysicsService>()) {
                    physics->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
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
                        physics->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
                    } else if (auto p = dm->GetService<PhysicsService>()) {
                        p->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
                    }
                }
            }
        } else {
            // Detached from tree
            if (!physicsBodyID.IsInvalid()) {
                if (auto physics = registeredService.lock()) {
                    physics->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
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

        JPH::BodyInterface &bi = physics->GetPhysicsSystem()->GetBodyInterface();

        if (name == "CFrame") {
            auto cf = basePartProps->CFrame.get().to_nova();
            glm::quat q = glm::normalize(glm::quat_cast(cf.rotation));
            bi.SetPositionAndRotation(physicsBodyID, 
                JPH::RVec3(cf.position.x, cf.position.y, cf.position.z),
                JPH::Quat(q.x, q.y, q.z, q.w),
                JPH::EActivation::Activate);
        }
        else if (name == "Anchored") {
            bool anchored = basePartProps->Anchored;
            JPH::EMotionType motionType = anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
            JPH::ObjectLayer layer = anchored ? Layers::NON_MOVING : Layers::MOVING;
            
            bi.SetMotionType(physicsBodyID, motionType, JPH::EActivation::Activate);
            bi.SetObjectLayer(physicsBodyID, layer);
            
            physics->RequestAssemblyUpdate(this);
        }
        else if (name == "Size") {
            physics->UnregisterPart(std::static_pointer_cast<BasePart>(shared_from_this()));
            physics->BulkRegisterParts({ std::static_pointer_cast<BasePart>(shared_from_this()) });
        }
    }
}
