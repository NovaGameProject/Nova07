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
#include <map>
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

    private:
        void SyncTransforms();


        // Jolt Boilerplate
        JPH::PhysicsSystem* physicsSystem;
        JPH::TempAllocatorImpl* tempAllocator;
        JPH::JobSystemThreadPool* jobSystem;

        // Mapping for state sync
        std::map<JPH::BodyID, std::weak_ptr<BasePart>> bodyToPartMap;

        // Threading
        std::thread mThread;
        std::atomic<bool> mStopping = false;
        std::mutex mPhysicsMutex; // Mutex for Jolt system access
        std::mutex mBufferMutex;  // Mutex for transform buffer
        std::vector<TransformUpdate> mTransformBuffer;

        // Implementation of BroadPhaseLayerInterface, ObjectVsBroadPhaseLayerFilter, etc.
        class BPLInterfaceImpl;
        class ObjectVsBroadPhaseLayerFilterImpl;
        class ObjectLayerPairFilterImpl;

        std::unique_ptr<BPLInterfaceImpl> bp_interface;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> obp_filter;
        std::unique_ptr<ObjectLayerPairFilterImpl> olp_filter;
    };
}
