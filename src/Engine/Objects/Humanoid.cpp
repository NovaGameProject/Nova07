// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/Humanoid.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Common/Log.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Nova {

    Humanoid::Humanoid() : Instance("Humanoid") {
        // Create body parts
        headPart = std::make_shared<Part>("Head");
        torsoPart = std::make_shared<Part>("HumanoidRootPart");
        leftArmPart = std::make_shared<Part>("Left Arm");
        rightArmPart = std::make_shared<Part>("Right Arm");
        leftLegPart = std::make_shared<Part>("Left Leg");
        rightLegPart = std::make_shared<Part>("Right Leg");

        // Set R6 sizes
        torsoPart->size = {2.0f, 2.0f, 1.0f};
        headPart->size = {2.0f, 1.0f, 1.0f};
        leftArmPart->size = {1.0f, 2.0f, 1.0f};
        rightArmPart->size = {1.0f, 2.0f, 1.0f};
        leftLegPart->size = {1.0f, 2.0f, 1.0f};
        rightLegPart->size = {1.0f, 2.0f, 1.0f};

        // All parts are unanchored and not collidable (we manage physics manually)
        headPart->anchored = false;
        torsoPart->anchored = false;
        leftArmPart->anchored = false;
        rightArmPart->anchored = false;
        leftLegPart->anchored = false;
        rightLegPart->anchored = false;

        // Disable auto-collision for humanoid parts (they don't merge into assemblies)
        headPart->canCollide = false;
        torsoPart->canCollide = false;
        leftArmPart->canCollide = false;
        rightArmPart->canCollide = false;
        leftLegPart->canCollide = false;
        rightLegPart->canCollide = false;
    }

    Humanoid::~Humanoid() {
        CleanupPhysics();
    }

    void Humanoid::InitializePhysics(std::shared_ptr<PhysicsService> physics) {
        if (physicsInitialized) return;

        physicsService = physics;
        auto* ps = physics.get();
        auto* system = ps->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        // Create body parts at default positions (will be repositioned on Respawn)
        CreateBodyParts({0, 10, 0});

        // Create Jolt bodies for each part
        auto createBody = [&](std::shared_ptr<Part>& part, bool isDynamic) {
            glm::vec3 halfExtents = part->size * 0.5f;
            JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
            auto shapeResult = shapeSettings.Create();
            if (!shapeResult.IsValid()) return;

            JPH::EMotionType motionType = isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
            JPH::BodyCreationSettings bodySettings(
                shapeResult.Get(),
                JPH::RVec3(part->cframe.position.x, part->cframe.position.y, part->cframe.position.z),
                JPH::Quat::sIdentity(),
                motionType,
                Layers::CHARACTER
            );
            bodySettings.mAllowSleeping = false;
            bodySettings.mFriction = 0.5f;
            bodySettings.mRestitution = 0.1f;
            bodySettings.mLinearDamping = 0.1f;
            bodySettings.mAngularDamping = ANGULAR_DAMPING;
            bodySettings.mGravityFactor = 1.0f;

            JPH::Body* body = bi.CreateBody(bodySettings);
            if (body) {
                part->physicsBodyID = body->GetID();
                bi.AddBody(body->GetID(), JPH::EActivation::Activate);
                ps->mAllActiveBodies.insert(body->GetID());
            }
        };

        // Create all bodies as dynamic
        createBody(torsoPart, true);
        createBody(headPart, true);
        createBody(leftArmPart, true);
        createBody(rightArmPart, true);
        createBody(leftLegPart, true);
        createBody(rightLegPart, true);

        // Create joints
        CreateJoints();

        physicsInitialized = true;
        LOG_INF("Humanoid", "Physics initialized for humanoid '%s'", m_debugName.c_str());
    }

    void Humanoid::CleanupPhysics() {
        if (!physicsInitialized) return;

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        // Destroy joints first
        DestroyJoints();

        // Remove and destroy all bodies
        auto removeBody = [&](std::shared_ptr<Part>& part) {
            if (!part->physicsBodyID.IsInvalid()) {
                bi.RemoveBody(part->physicsBodyID);
                bi.DestroyBody(part->physicsBodyID);
                physics->mAllActiveBodies.erase(part->physicsBodyID);
                part->physicsBodyID = JPH::BodyID();
            }
        };

        removeBody(torsoPart);
        removeBody(headPart);
        removeBody(leftArmPart);
        removeBody(rightArmPart);
        removeBody(leftLegPart);
        removeBody(rightLegPart);

        physicsInitialized = false;
    }

    void Humanoid::CreateBodyParts(Vector3 spawnPosition) {
        // Position body parts relative to spawn
        // Torso at spawn position
        torsoPart->cframe.position = spawnPosition;

        // Head above torso (torso half-height + head half-height + small gap)
        headPart->cframe.position = spawnPosition + Vector3(0, 1.5f, 0);

        // Arms at torso sides
        leftArmPart->cframe.position = spawnPosition + Vector3(-1.5f, 0, 0);
        rightArmPart->cframe.position = spawnPosition + Vector3(1.5f, 0, 0);

        // Legs below torso
        leftLegPart->cframe.position = spawnPosition + Vector3(-0.5f, -2.0f, 0);
        rightLegPart->cframe.position = spawnPosition + Vector3(0.5f, -2.0f, 0);
    }

    void Humanoid::CreateJoints() {
        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        auto& bi = system->GetBodyInterface();

        // Helper to create a SwingTwistConstraint between two parts
        auto createJoint = [&](std::shared_ptr<Part>& parent, std::shared_ptr<Part>& child,
                               float normalHalfCone, float planeHalfCone,
                               float twistMin, float twistMax) -> JPH::SwingTwistConstraint* {
            if (parent->physicsBodyID.IsInvalid() || child->physicsBodyID.IsInvalid()) return nullptr;

            // Calculate joint position (midpoint between parts)
            glm::vec3 parentPos = parent->cframe.position;
            glm::vec3 childPos = child->cframe.position;
            glm::vec3 jointPos = (parentPos + childPos) * 0.5f;

            JPH::SwingTwistConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mPosition1 = JPH::RVec3(jointPos.x, jointPos.y, jointPos.z);
            settings.mPosition2 = JPH::RVec3(jointPos.x, jointPos.y, jointPos.z);

            // Twist axis points from parent to child
            glm::vec3 twistAxis = glm::normalize(childPos - parentPos);
            settings.mTwistAxis1 = JPH::Vec3(twistAxis.x, twistAxis.y, twistAxis.z);
            settings.mTwistAxis2 = JPH::Vec3(twistAxis.x, twistAxis.y, twistAxis.z);

            // Plane axis (perpendicular to twist, pointing "forward")
            glm::vec3 planeAxis = glm::normalize(glm::cross(twistAxis, glm::vec3(0, 0, 1)));
            if (glm::length(planeAxis) < 0.001f) {
                planeAxis = glm::normalize(glm::cross(twistAxis, glm::vec3(1, 0, 0)));
            }
            settings.mPlaneAxis1 = JPH::Vec3(planeAxis.x, planeAxis.y, planeAxis.z);
            settings.mPlaneAxis2 = JPH::Vec3(planeAxis.x, planeAxis.y, planeAxis.z);

            // Set swing/twist limits
            settings.mNormalHalfConeAngle = normalHalfCone * (float)(M_PI / 180.0f);
            settings.mPlaneHalfConeAngle = planeHalfCone * (float)(M_PI / 180.0f);
            settings.mTwistMinAngle = twistMin * (float)(M_PI / 180.0f);
            settings.mTwistMaxAngle = twistMax * (float)(M_PI / 180.0f);

            // Motor settings (spring-damped to keep limbs in pose)
            settings.mSwingMotorSettings.mSpringSettings.mFrequency = 4.0f;
            settings.mSwingMotorSettings.mSpringSettings.mDamping = 0.7f;
            settings.mTwistMotorSettings.mSpringSettings.mFrequency = 4.0f;
            settings.mTwistMotorSettings.mSpringSettings.mDamping = 0.7f;

            // Lock bodies and create constraint
            JPH::BodyID ids[] = { parent->physicsBodyID, child->physicsBodyID };
            JPH::BodyLockMultiWrite multiLock(system->GetBodyLockInterface(), ids, 2);
            if (!multiLock.GetBody(0) || !multiLock.GetBody(1)) return nullptr;

            auto constraint = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
            if (constraint) {
                system->AddConstraint(constraint);
                // Cast to SwingTwistConstraint to enable motors
                auto* swingTwist = static_cast<JPH::SwingTwistConstraint*>(constraint);
                swingTwist->SetSwingMotorState(JPH::EMotorState::Position);
                swingTwist->SetTwistMotorState(JPH::EMotorState::Position);
                return swingTwist;
            }
            return nullptr;
        };

        // Create joints: Torso is the parent for all limbs
        // Head: tight cone (30° swing, ±20° twist)
        limbJoints[0] = createJoint(torsoPart, headPart, 30.0f, 30.0f, -20.0f, 20.0f);

        // Shoulders: wider cone (90° swing, ±45° twist)
        limbJoints[1] = createJoint(torsoPart, leftArmPart, 90.0f, 90.0f, -45.0f, 45.0f);
        limbJoints[2] = createJoint(torsoPart, rightArmPart, 90.0f, 90.0f, -45.0f, 45.0f);

        // Hips: medium cone (70° swing, ±30° twist)
        limbJoints[3] = createJoint(torsoPart, leftLegPart, 70.0f, 70.0f, -30.0f, 30.0f);
        limbJoints[4] = createJoint(torsoPart, rightLegPart, 70.0f, 70.0f, -30.0f, 30.0f);
    }

    void Humanoid::DestroyJoints() {
        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();

        for (auto*& joint : limbJoints) {
            if (joint) {
                system->RemoveConstraint(joint);
                joint = nullptr;
            }
        }
    }

    void Humanoid::TakeDamage(float amount) {
        if (isDead) return;

        Health -= amount;
        HealthChanged.fire();

        if (Health <= 0.0f) {
            Health = 0.0f;
            isDead = true;
            respawnTimer = 5.0f;  // 5 second respawn timer

            // Break all joints - parts fall like Legos!
            DestroyJoints();

            Died.fire();
            LOG_INF("Humanoid", "Humanoid '%s' died!", m_debugName.c_str());
        }
    }

    void Humanoid::Move(Vector3 direction) {
        MoveDirection = direction;
    }

    void Humanoid::Jump() {
        if (isDead || !grounded) return;

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        if (!torsoPart->physicsBodyID.IsInvalid()) {
            glm::vec3 currentVel = torsoPart->GetVelocity();
            bi.SetLinearVelocity(torsoPart->physicsBodyID,
                JPH::Vec3(currentVel.x, JumpPower, currentVel.z));
        }
    }

    void Humanoid::Respawn(Vector3 position) {
        if (!physicsInitialized) return;

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        // If we have joints, destroy them first
        DestroyJoints();

        // Reposition body parts
        CreateBodyParts(position);

        // Teleport all bodies to new positions
        auto teleportBody = [&](std::shared_ptr<Part>& part) {
            if (!part->physicsBodyID.IsInvalid()) {
                bi.SetPosition(part->physicsBodyID,
                    JPH::RVec3(part->cframe.position.x, part->cframe.position.y, part->cframe.position.z),
                    JPH::EActivation::Activate);
                bi.SetRotation(part->physicsBodyID, JPH::Quat::sIdentity(), JPH::EActivation::Activate);
                bi.SetLinearVelocity(part->physicsBodyID, JPH::Vec3::sZero());
                bi.SetAngularVelocity(part->physicsBodyID, JPH::Vec3::sZero());
            }
        };

        teleportBody(torsoPart);
        teleportBody(headPart);
        teleportBody(leftArmPart);
        teleportBody(rightArmPart);
        teleportBody(leftLegPart);
        teleportBody(rightLegPart);

        // Re-create joints
        CreateJoints();

        // Reset state
        Health = MaxHealth;
        isDead = false;
        grounded = false;
        MoveDirection = {0, 0, 0};

        LOG_INF("Humanoid", "Humanoid '%s' respawned at (%.1f, %.1f, %.1f)",
            m_debugName.c_str(), position.x, position.y, position.z);
    }

    void Humanoid::Update(float dt) {
        if (!physicsInitialized) return;

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        // Handle respawn timer
        if (isDead) {
            respawnTimer -= dt;
            if (respawnTimer <= 0.0f) {
                Respawn({0, 10, 0});  // Default spawn position
            }
            return;
        }

        // Check if grounded
        CheckGrounded();

        // Apply keep-upright impulse
        ApplyKeepUpright(dt);

        // Apply movement velocity
        if (grounded && glm::length(MoveDirection) > 0.001f) {
            glm::vec3 moveDir = glm::normalize(MoveDirection);
            glm::vec3 velocity = moveDir * WalkSpeed;

            // Preserve vertical velocity
            glm::vec3 currentVel = torsoPart->GetVelocity();
            bi.SetLinearVelocity(torsoPart->physicsBodyID,
                JPH::Vec3(velocity.x, currentVel.y, velocity.z));
        } else if (grounded) {
            // Stop horizontal movement when no input
            glm::vec3 currentVel = torsoPart->GetVelocity();
            bi.SetLinearVelocity(torsoPart->physicsBodyID,
                JPH::Vec3(0, currentVel.y, 0));
        }

        // Sync Part transforms from physics
        SyncPartTransforms();
    }

    void Humanoid::ApplyKeepUpright(float dt) {
        if (isDead) return;

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        if (torsoPart->physicsBodyID.IsInvalid()) return;

        // Get current torso orientation
        JPH::Quat rotation = bi.GetRotation(torsoPart->physicsBodyID);
        glm::quat currentRot(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());

        // Calculate angle from upright (world Y axis)
        glm::vec3 up = currentRot * glm::vec3(0, 1, 0);
        float dotProduct = glm::dot(up, glm::vec3(0, 1, 0));
        uprightAngle = glm::degrees(acosf(glm::clamp(dotProduct, -1.0f, 1.0f)));

        // If tipped too far, don't apply movement (character is "fallen")
        if (uprightAngle > MAX_UPRIGHT_ANGLE) {
            // Still apply upright impulse to recover
        }

        // Calculate torque to bring torso back to upright
        // Cross product of current up and desired up gives rotation axis
        glm::vec3 rotationAxis = glm::cross(up, glm::vec3(0, 1, 0));
        float axisLength = glm::length(rotationAxis);

        if (axisLength > 0.001f) {
            rotationAxis = glm::normalize(rotationAxis);

            // Scale impulse by how far we're tipped
            float tipFactor = uprightAngle / 90.0f;  // 0 to 1
            glm::vec3 impulse = rotationAxis * UPRIGHT_STRENGTH * tipFactor * dt;

            // Apply angular impulse
            bi.AddAngularImpulse(torsoPart->physicsBodyID,
                JPH::Vec3(impulse.x, impulse.y, impulse.z));
        }

        // Apply additional angular damping to prevent spinning
        JPH::Vec3 angVel = bi.GetAngularVelocity(torsoPart->physicsBodyID);
        bi.SetAngularVelocity(torsoPart->physicsBodyID,
            JPH::Vec3(angVel.GetX() * ANGULAR_DAMPING,
                       angVel.GetY() * ANGULAR_DAMPING,
                       angVel.GetZ() * ANGULAR_DAMPING));
    }

    void Humanoid::CheckGrounded() {
        if (isDead) {
            grounded = false;
            return;
        }

        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        if (torsoPart->physicsBodyID.IsInvalid()) return;

        // Simple ground check: cast a ray downward from torso
        JPH::RVec3 position = bi.GetPosition(torsoPart->physicsBodyID);

        // Check if there's something below us within a small distance
        JPH::RRayCast ray;
        ray.mOrigin = position;
        ray.mDirection = JPH::Vec3(0, -2.5f, 0);  // Cast 2.5 units down

        JPH::RayCastResult hit;
        grounded = system->GetNarrowPhaseQuery().CastRay(ray, hit);
    }

    void Humanoid::SyncPartTransforms() {
        auto physics = physicsService.lock();
        if (!physics) return;

        auto* system = physics->GetPhysicsSystem();
        JPH::BodyInterface& bi = system->GetBodyInterface();

        // Update each part's cframe from its physics body
        auto syncPart = [&](std::shared_ptr<Part>& part) {
            if (part->physicsBodyID.IsInvalid()) return;

            JPH::RVec3 pos = bi.GetPosition(part->physicsBodyID);
            JPH::Quat rot = bi.GetRotation(part->physicsBodyID);

            part->cframe.position = {pos.GetX(), pos.GetY(), pos.GetZ()};
            part->cframe.rotation = glm::mat3_cast(
                glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));
        };

        syncPart(torsoPart);
        syncPart(headPart);
        syncPart(leftArmPart);
        syncPart(rightArmPart);
        syncPart(leftLegPart);
        syncPart(rightLegPart);
    }

} // namespace Nova
