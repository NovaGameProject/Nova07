// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "Engine/Objects/Instance.hpp"
#include "Engine/Objects/Part.hpp"
#include "Engine/Common/Signal.hpp"
#include "Common/MathTypes.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <memory>
#include <array>

namespace Nova {
    class PhysicsService;

    class Humanoid : public Instance {
    public:
        // Properties (Lua-accessible, replicatable)
        float Health = 100.0f;
        float MaxHealth = 100.0f;
        float WalkSpeed = 16.0f;
        float JumpPower = 50.0f;
        Vector3 MoveDirection;

        // Signals
        Signal HealthChanged;
        Signal Died;
        Signal Touched;

        Humanoid();
        ~Humanoid();

        std::string GetClassName() const override { return "Humanoid"; }
        std::string GetName() const override { return m_debugName; }

        // Methods
        void TakeDamage(float amount);
        void Move(Vector3 direction);
        void Jump();
        void Respawn(Vector3 position);

        // Per-frame update (called by PhysicsService)
        void Update(float dt);

        // Get the torso (HumanoidRootPart)
        std::shared_ptr<Part> GetTorso() const { return torsoPart; }

        // Check if character is on ground
        bool IsGrounded() const { return grounded; }

        // Check if character is dead
        bool IsDead() const { return isDead; }

        // Initialize physics bodies and joints
        void InitializePhysics(std::shared_ptr<PhysicsService> physics);

        // Cleanup physics
        void CleanupPhysics();

    private:
        // R6 body parts
        std::shared_ptr<Part> headPart;
        std::shared_ptr<Part> torsoPart;
        std::shared_ptr<Part> leftArmPart;
        std::shared_ptr<Part> rightArmPart;
        std::shared_ptr<Part> leftLegPart;
        std::shared_ptr<Part> rightLegPart;

        // Joint constraints (for breaking on death)
        std::array<JPH::SwingTwistConstraint*, 5> limbJoints = {};

        // Physics state
        std::weak_ptr<PhysicsService> physicsService;
        bool physicsInitialized = false;

        // Character state
        bool isDead = false;
        bool grounded = false;
        float respawnTimer = 0.0f;
        float uprightAngle = 0.0f;  // Current angle from upright

        // Keep-upright parameters
        static constexpr float MAX_UPRIGHT_ANGLE = 80.0f;  // Degrees - beyond this, character is "fallen"
        static constexpr float UPRIGHT_STRENGTH = 50.0f;   // Angular impulse strength
        static constexpr float ANGULAR_DAMPING = 0.9f;     // Angular velocity damping

        void CreateBodyParts(Vector3 spawnPosition);
        void CreateJoints();
        void DestroyJoints();
        void ApplyKeepUpright(float dt);
        void CheckGrounded();
        void SyncPartTransforms();

        void ForEachPart(auto&& fn) {
            fn(torsoPart); fn(headPart);
            fn(leftArmPart); fn(rightArmPart);
            fn(leftLegPart); fn(rightLegPart);
        }
    };
}
