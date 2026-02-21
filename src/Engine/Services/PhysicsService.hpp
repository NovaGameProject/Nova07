// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include "Common/MathTypes.hpp"
#include "Engine/Physics/Assembly.hpp"
#include "Engine/Physics/JoltLayers.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <atomic>

namespace Nova {

    class BasePart;
    class JointInstance;
    enum class SurfaceType : int;
    class ContactListenerImpl;

    struct TransformUpdate {
        std::weak_ptr<BasePart> part;
        glm::vec3 position;
        glm::quat rotation;
    };

    struct ContactEvent {
        std::weak_ptr<BasePart> part1;
        std::weak_ptr<BasePart> part2;
    };

    struct JointRequest {
        std::weak_ptr<BasePart> part1;
        std::weak_ptr<BasePart> part2;
        SurfaceType surface1;
        SurfaceType surface2;
        JPH::Constraint* physicsConstraint = nullptr;
    };

    class PhysicsService : public Instance {
    public:
        PhysicsService();
        ~PhysicsService();

        NOVA_OBJECT_NO_PROPS(PhysicsService)

        // Async Physics Management
        void Start();
        void Stop();

        // Called by Main Thread to apply queued updates
        void Step(float dt);

        // Physics Management (Now non-blocking)
        void BulkRegisterParts(const std::vector<std::shared_ptr<BasePart>>& parts);
        void BulkUnregisterParts(const std::vector<BasePart*>& parts);
        void UnregisterPart(BasePart* part);

        // Joint Management
        void RegisterConstraint(JointInstance* joint);
        void UnregisterConstraint(JointInstance* joint);

        JPH::PhysicsSystem* GetPhysicsSystem() { return physicsSystem; }
        std::recursive_mutex& GetPhysicsMutex() { return mPhysicsMutex; }

        // Performance Optimization: Defer registration during level load
        void SetDeferRegistration(bool defer);
        bool IsDeferring() const { return mDeferring; }

        // Deduplication helper
        bool HasJointBetween(BasePart* p1, BasePart* p2);

        // Assembly Management
        void RequestAssemblyUpdate(BasePart* part);
        void BreakJoints(BasePart* part);
        void BreakJointsInRadius(glm::vec3 position, float radius);
        
        // Queue explosion for processing on physics thread (thread-safe)
        void QueueExplosion(glm::vec3 position, float radius, float pressure);
        
        // Explosion impulse - returns list of (part, distance) pairs for Hit signal
        std::vector<std::pair<std::shared_ptr<BasePart>, float>> ApplyExplosionImpulse(
            glm::vec3 position, float radius, float pressure);

        struct InternalJoint {
            std::weak_ptr<BasePart> part1;
            std::weak_ptr<BasePart> part2;
            JPH::Constraint* physicsConstraint = nullptr;
        };

        // Mapping for state sync (using unordered_map for O(1) average lookup)
        struct BodyIDHasher {
            size_t operator()(const JPH::BodyID& id) const {
                return std::hash<uint32_t>{}(id.GetIndex());
            }
        };
        std::unordered_map<JPH::BodyID, std::shared_ptr<Assembly>, BodyIDHasher> mBodyToAssembly;
        std::unordered_map<BasePart*, std::shared_ptr<Assembly>> mPartToAssembly;
        mutable std::shared_mutex mMapsMutex;

        // Publicly accessible buffers for the contact listener/internal managers
        std::mutex mContactMutex;
        std::vector<ContactEvent> mContactBuffer;
        
        std::recursive_mutex mQueueMutex; 
        std::vector<std::shared_ptr<BasePart>> mPendingRegisters;
        std::vector<JPH::BodyID> mPendingRemovals;
        std::vector<std::shared_ptr<JointInstance>> mPendingConstraints;
        std::vector<JPH::Constraint*> mPendingConstraintRemovals;
        std::vector<JointRequest> mPendingAutoJoints;
        std::vector<std::shared_ptr<InternalJoint>> mInternalJointsToRemove;
        std::vector<std::shared_ptr<InternalJoint>> mActiveAutoJoints;
        std::vector<std::weak_ptr<BasePart>> mPendingAssemblyUpdates;
        std::vector<std::shared_ptr<JointInstance>> mPendingJointDestructions; // For thread-safe scene tree cleanup

        using PartPair = std::pair<uint64_t, uint64_t>;
        struct PartPairHasher {
            size_t operator()(const PartPair& p) const {
                return std::hash<uint64_t>{}(p.first) ^ (std::hash<uint64_t>{}(p.second) << 1);
            }
        };
        std::unordered_set<PartPair, PartPairHasher> mJoinedPairs;
        mutable std::shared_mutex mJoinedPairsMutex;

        std::unordered_map<BasePart*, std::vector<std::weak_ptr<JointInstance>>> mPartToJoints;
        std::unordered_map<BasePart*, std::vector<std::shared_ptr<InternalJoint>>> mPartToAutoJoints;

        // Tracks ALL bodies currently alive in Jolt
        std::unordered_set<JPH::BodyID, BodyIDHasher> mAllActiveBodies;

    private:
        void SyncTransforms();
        void ProcessExplosions();    
        void ProcessQueuedMutations(); 
        void UpdateAssemblies();       

        // Jolt Boilerplate
        JPH::PhysicsSystem* physicsSystem;
        JPH::TempAllocatorImpl* tempAllocator;
        JPH::JobSystemThreadPool* jobSystem;

        // Threading
        std::thread mThread;
        std::atomic<bool> mStopping = false;
        std::recursive_mutex mPhysicsMutex; 
        
        std::mutex mBufferMutex;  
        std::vector<TransformUpdate> mTransformBuffer;

        // Explosion requests
        struct ExplosionRequest {
            glm::vec3 position;
            float radius;
            float pressure;
        };
        std::vector<ExplosionRequest> mPendingExplosions;

        // Deferred registration state
        bool mDeferring = false;
        std::vector<std::shared_ptr<BasePart>> mDeferredParts;

        // Jolt implementation classes
        std::unique_ptr<BPLInterfaceImpl> bp_interface;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> obp_filter;
        std::unique_ptr<ObjectLayerPairFilterImpl> olp_filter;
        std::unique_ptr<ContactListenerImpl> contact_listener;
    };
}
