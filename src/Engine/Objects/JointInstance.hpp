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
    class JointInstance : public Instance {
    public:
        CFrame c0;
        CFrame c1;
        std::weak_ptr<BasePart> Part0;
        std::weak_ptr<BasePart> Part1;

        JPH::Constraint* physicsConstraint = nullptr;
        std::weak_ptr<PhysicsService> registeredService;

        JointInstance(std::string name) : Instance(name) {}
        virtual ~JointInstance();

        void OnAncestorChanged(std::shared_ptr<Instance> instance, std::shared_ptr<Instance> newParent) override;
        virtual void RebuildConstraint();

        std::string GetClassName() const override { return "JointInstance"; }
        std::string GetName() const override { return m_debugName; }
    };

    class AutoJoint : public JointInstance {
    public:
        AutoJoint() : JointInstance("AutoJoint") {}
        void RebuildConstraint() override;
        std::string GetClassName() const override { return "AutoJoint"; }
    };

    class Weld : public JointInstance {
    public:
        Weld() : JointInstance("Weld") {}
        void RebuildConstraint() override;
        std::string GetClassName() const override { return "Weld"; }
    };

    class Snap : public JointInstance {
    public:
        Snap() : JointInstance("Snap") {}
        void RebuildConstraint() override;
        std::string GetClassName() const override { return "Snap"; }
    };

    class Glue : public JointInstance {
    public:
        Glue() : JointInstance("Glue") {}
        void RebuildConstraint() override;
        std::string GetClassName() const override { return "Glue"; }
    };

    class Motor : public JointInstance {
    public:
        float MaxVelocity = 1.0f;
        float DesiredAngle = 0.0f;

        Motor() : JointInstance("Motor") {}
        void RebuildConstraint() override;
        std::string GetClassName() const override { return "Motor"; }
    };

    class Hinge : public JointInstance {
    public:
        float LowerAngle = 0.0f;
        float UpperAngle = 0.0f;
        bool LimitsEnabled = false;

        Hinge() : JointInstance("Hinge") {}
        void RebuildConstraint() override;
        float GetCurrentAngle();
        std::string GetClassName() const override { return "Hinge"; }
    };

    class VelocityMotor : public JointInstance {
    public:
        float MaxVelocity = 1.0f;
        float DesiredAngle = 0.0f;

        VelocityMotor() : JointInstance("VelocityMotor") {}
        void RebuildConstraint() override;
        float GetCurrentAngle();
        void SetTargetVelocity(float velocity);
        std::string GetClassName() const override { return "VelocityMotor"; }
    };
}
