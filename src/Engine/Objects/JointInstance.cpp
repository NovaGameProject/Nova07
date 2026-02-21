// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/JointInstance.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/Workspace.hpp"
#include <Jolt/Physics/Constraints/HingeConstraint.h>

namespace Nova {

    JointInstance::~JointInstance() {
        if (physicsConstraint) {
            if (auto physics = registeredService.lock()) {
                physics->UnregisterConstraint(this);
            }
        }
    }

    void JointInstance::OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) {
        Instance::OnAncestorChanged(instance, newParent);

        auto dm = GetDataModel();
        if (dm) {
            auto workspace = dm->GetService<Workspace>();
            if (IsDescendantOf(workspace)) {
                RebuildConstraint();
            } else {
                if (physicsConstraint) {
                    if (auto physics = registeredService.lock()) {
                        physics->UnregisterConstraint(this);
                    }
                }
            }
        }
    }

    void JointInstance::RebuildConstraint() {}

    void AutoJoint::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }

    void Weld::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }

    void Snap::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }

    void Glue::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }

    void Motor::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }

    void Hinge::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }
    
    float Hinge::GetCurrentAngle() {
        if (!physicsConstraint) return 0.0f;
        
        auto physics = registeredService.lock();
        if (!physics) return 0.0f;
        
        // Cast to hinge constraint and get angle
        auto* hinge = static_cast<JPH::HingeConstraint*>(physicsConstraint);
        return hinge->GetCurrentAngle();
    }

    void VelocityMotor::RebuildConstraint() {
        if (auto dm = GetDataModel()) {
            if (auto physics = dm->GetService<PhysicsService>()) {
                physics->RegisterConstraint(this);
            }
        }
    }
    
    float VelocityMotor::GetCurrentAngle() {
        if (!physicsConstraint) return 0.0f;
        
        auto physics = registeredService.lock();
        if (!physics) return 0.0f;
        
        auto* hinge = static_cast<JPH::HingeConstraint*>(physicsConstraint);
        return hinge->GetCurrentAngle();
    }
    
    void VelocityMotor::SetTargetVelocity(float velocity) {
        if (!physicsConstraint) return;
        
        auto physics = registeredService.lock();
        if (!physics) return;
        
        auto* hinge = static_cast<JPH::HingeConstraint*>(physicsConstraint);
        hinge->SetMotorState(JPH::EMotorState::Velocity);
        hinge->SetTargetAngularVelocity(velocity);
    }

}
