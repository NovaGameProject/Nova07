// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include "Engine/Objects/Instance.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

namespace Nova {

    class BasePart;

    // Layer definitions for Jolt
    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    struct TransformUpdate {
        std::weak_ptr<BasePart> part;
        glm::vec3 position;
        glm::quat rotation;
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

        // Physics Management
        void BulkRegisterParts(const std::vector<std::shared_ptr<BasePart>>& parts);
        void UnregisterPart(std::shared_ptr<BasePart> part);

        JPH::PhysicsSystem* GetPhysicsSystem() { return physicsSystem; }

        // Performance Optimization: Defer registration during level load
        void SetDeferRegistration(bool defer);
        bool IsDeferring() const { return mDeferring; }

    private:
        void SyncTransforms();


        // Jolt Boilerplate
        JPH::PhysicsSystem* physicsSystem;
        JPH::TempAllocatorImpl* tempAllocator;
        JPH::JobSystemThreadPool* jobSystem;

        // Mapping for state sync (using unordered_map for O(1) average lookup)
        struct BodyIDHasher {
            size_t operator()(const JPH::BodyID& id) const {
                return std::hash<uint32_t>{}(id.GetIndex());
            }
        };
        std::unordered_map<JPH::BodyID, std::weak_ptr<BasePart>, BodyIDHasher> bodyToPartMap;

        // Threading
        std::thread mThread;
        std::atomic<bool> mStopping = false;
        std::recursive_mutex mPhysicsMutex; // Changed to recursive_mutex to prevent deadlocks on nested calls
        std::mutex mBufferMutex;  // Mutex for transform buffer
        std::vector<TransformUpdate> mTransformBuffer;

        // Deferred registration state
        bool mDeferring = false;
        std::vector<std::shared_ptr<BasePart>> mDeferredParts;

        // Implementation of BroadPhaseLayerInterface, ObjectVsBroadPhaseLayerFilter, etc.
        class BPLInterfaceImpl;
        class ObjectVsBroadPhaseLayerFilterImpl;
        class ObjectLayerPairFilterImpl;

        std::unique_ptr<BPLInterfaceImpl> bp_interface;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> obp_filter;
        std::unique_ptr<ObjectLayerPairFilterImpl> olp_filter;
    };
}
