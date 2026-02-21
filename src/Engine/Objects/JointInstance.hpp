// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Common/MathTypes.hpp"
#include <Jolt/Physics/Constraints/Constraint.h>

namespace Nova {

    namespace Props {
        struct JointProps {
            rfl::Flatten<InstanceProps> base;
            rfl::Rename<"C0", CFrameReflect> C0;
            rfl::Rename<"C1", CFrameReflect> C1;
        };
    }

    class JointInstance : public Instance {
    public:
        virtual ~JointInstance();

        std::weak_ptr<BasePart> Part0;
        std::weak_ptr<BasePart> Part1;

        // Jolt constraint handle
        JPH::Constraint* physicsConstraint = nullptr;
        std::weak_ptr<PhysicsService> registeredService;

        JointInstance(std::string name) : Instance(name) {}

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;
        virtual void RebuildConstraint();
    };

    // Internal class for surface-based joints
    class AutoJoint : public JointInstance {
    public:
        Props::JointProps props;
        NOVA_OBJECT(AutoJoint, props)
        AutoJoint() : JointInstance("AutoJoint") {}
        void RebuildConstraint() override;
    };

    // Specific Joint Types

    class Weld : public JointInstance {
    public:
        Props::JointProps props;
        NOVA_OBJECT(Weld, props)
        Weld() : JointInstance("Weld") {}
        void RebuildConstraint() override;
    };

    class Snap : public JointInstance {
    public:
        Props::JointProps props;
        NOVA_OBJECT(Snap, props)
        Snap() : JointInstance("Snap") {}
        void RebuildConstraint() override;
    };

    class Glue : public JointInstance {
    public:
        Props::JointProps props;
        NOVA_OBJECT(Glue, props)
        Glue() : JointInstance("Glue") {}
        void RebuildConstraint() override;
    };

    namespace Props {
        struct MotorProps {
            rfl::Flatten<JointProps> base;
            float MaxVelocity = 1.0f;
            float DesiredAngle = 0.0f;
        };
    }

    class Motor : public JointInstance {
    public:
        Props::MotorProps props;
        NOVA_OBJECT(Motor, props)
        Motor() : JointInstance("Motor") {}
        void RebuildConstraint() override;
    };

    // Hinge joint - allows rotation about one axis with optional limits
    namespace Props {
        struct HingeProps {
            rfl::Flatten<JointProps> base;
            float LowerAngle = 0.0f;
            float UpperAngle = 0.0f;
            bool LimitsEnabled = false;
        };
    }

    class Hinge : public JointInstance {
    public:
        Props::HingeProps props;
        NOVA_OBJECT(Hinge, props)
        Hinge() : JointInstance("Hinge") {}
        void RebuildConstraint() override;
        
        float GetCurrentAngle();
    };

    // VelocityMotor - motor with velocity control (2007 ROBLOX style)
    namespace Props {
        struct VelocityMotorProps {
            rfl::Flatten<JointProps> base;
            float MaxVelocity = 1.0f;
            float DesiredAngle = 0.0f;
        };
    }

    class VelocityMotor : public JointInstance {
    public:
        Props::VelocityMotorProps props;
        NOVA_OBJECT(VelocityMotor, props)
        VelocityMotor() : JointInstance("VelocityMotor") {}
        void RebuildConstraint() override;
        
        float GetCurrentAngle();
        void SetTargetVelocity(float velocity);
    };

}
