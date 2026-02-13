// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "PhysicsService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/DataModel.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <SDL3/SDL_log.h>
#include <cstdarg>
#include <thread>

namespace Nova {

    // --- Jolt Callbacks ---

    static void TraceImpl(const char *inFMT, ...) {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        SDL_Log("Jolt: %s", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "JPH_ASSERT FAILED: %s (%s) at %s:%u", inExpression, inMessage ? inMessage : "no message", inFile, inLine);
        return true; // Trap
    }
#endif

    // --- Jolt Filter Implementations ---

    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr uint NUM_LAYERS = 2;
    };

    class PhysicsService::BPLInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLInterfaceImpl() {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }
        uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            return mObjectToBroadPhase[inLayer];
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            switch ((JPH::BroadPhaseLayer::Type)inLayer) {
                case 0: return "NON_MOVING";
                case 1: return "MOVING";
                default: return "INVALID";
            }
        }
#endif
    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class PhysicsService::ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inL1, JPH::ObjectLayer inL2) const override {
            switch (inL1) {
                case Layers::NON_MOVING:
                    return inL2 == Layers::MOVING;
                case Layers::MOVING:
                    // IMPORTANT: Moving must hit BOTH the floor and other moving parts
                    return inL2 == Layers::NON_MOVING || inL2 == Layers::MOVING;
                default: return false;
            }
        }
    };

    class PhysicsService::ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBroadPhaseLayer) const override {
            switch (inLayer) {
                case Layers::NON_MOVING:
                    return inBroadPhaseLayer == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    // IMPORTANT: Moving must look in both the Static and Moving buckets
                    return inBroadPhaseLayer == BroadPhaseLayers::NON_MOVING || inBroadPhaseLayer == BroadPhaseLayers::MOVING;
                default: return false;
            }
        }
    };

    class PhysicsService::ContactListenerImpl : public JPH::ContactListener {
    public:
        PhysicsService* service;
        ContactListenerImpl(PhysicsService* service) : service(service) {}

        JPH::ValidateResult OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override {
            // CRITICAL: OnContactAdded is called from Jolt worker threads during Update.
            // bodyToPartMap is STABLE during Update because no mutations occur.
            
            auto id1 = inBody1.GetID();
            auto id2 = inBody2.GetID();

            auto it1 = service->bodyToPartMap.find(id1);
            auto it2 = service->bodyToPartMap.find(id2);

            if (it1 != service->bodyToPartMap.end() && it2 != service->bodyToPartMap.end()) {
                auto p1 = it1->second.lock();
                auto p2 = it2->second.lock();

                if (p1 && p2) {
                    std::lock_guard<std::mutex> lock(service->mContactMutex);
                    service->mContactBuffer.push_back({ p1, p2 });
                }
            }
        }
    };

    // --- PhysicsService Implementation ---

    PhysicsService::PhysicsService() : Instance("PhysicsService") {
        JPH::RegisterDefaultAllocator();

        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

        if (!JPH::Factory::sInstance) {
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        tempAllocator = new JPH::TempAllocatorImpl(256 * 1024 * 1024); // Increased to 256MB
        jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::max(1, (int)std::thread::hardware_concurrency() - 1));

        bp_interface = std::make_unique<BPLInterfaceImpl>();
        obp_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
        olp_filter = std::make_unique<ObjectLayerPairFilterImpl>();
        contact_listener = std::make_unique<ContactListenerImpl>(this);

        physicsSystem = new JPH::PhysicsSystem();
        physicsSystem->Init(
            262144, // Max Bodies (Increased to 256k)
            2096,   // Mutexes
            262144, // Max Body Pairs
            262144, // Max Contact Manifolds
            *bp_interface,
            *obp_filter,
            *olp_filter
        );

        physicsSystem->SetContactListener(contact_listener.get());

        physicsSystem->SetGravity(JPH::Vec3(0, -196.2f, 0));
        JPH::PhysicsSettings settings;
        settings.mPointVelocitySleepThreshold = 2.0f;
        settings.mNumVelocitySteps = 4;
        settings.mMinVelocityForRestitution = 1.5f;
        settings.mTimeBeforeSleep = 0.5;
        settings.mNumPositionSteps = 2;
        settings.mBaumgarte = 0.2f;
        settings.mSpeculativeContactDistance = 0.05f;
        physicsSystem->SetPhysicsSettings(settings);
    }

    PhysicsService::~PhysicsService() {
        Stop();
        delete physicsSystem;
        delete jobSystem;
        delete tempAllocator;
    }

    void PhysicsService::Start() {
        if (mThread.joinable()) return;
        mStopping = false;
        mThread = std::thread([this]() {
            auto lastTime = std::chrono::steady_clock::now();

            while (!mStopping) {
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - lastTime).count();
                lastTime = now;

                if (dt > 0) {
                    std::lock_guard<std::recursive_mutex> lock(mPhysicsMutex);
                    
                    // Process mutations (Add/Remove) BEFORE update
                    ProcessQueuedMutations();

                    float internalDt = std::min(dt, 1.0f / 30.0f);
                    physicsSystem->Update(internalDt, 6, tempAllocator, jobSystem);
                    SyncTransforms();
                }

                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    void PhysicsService::Stop() {
        mStopping = true;
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    void PhysicsService::SetDeferRegistration(bool defer) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (mDeferring && !defer) {
            // Re-enabling: Process all deferred parts in one bulk call
            if (!mDeferredParts.empty()) {
                mPendingRegisters.insert(mPendingRegisters.end(), mDeferredParts.begin(), mDeferredParts.end());
                mDeferredParts.clear();
            }
        }
        mDeferring = defer;
    }

    void PhysicsService::BulkRegisterParts(const std::vector<std::shared_ptr<BasePart>>& parts) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (mDeferring) {
            mDeferredParts.insert(mDeferredParts.end(), parts.begin(), parts.end());
        } else {
            mPendingRegisters.insert(mPendingRegisters.end(), parts.begin(), parts.end());
        }
    }

    void PhysicsService::BulkUnregisterParts(const std::vector<std::shared_ptr<BasePart>>& parts) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        for (auto& part : parts) {
            if (!part->physicsBodyID.IsInvalid()) {
                mPendingRemovals.push_back(part->physicsBodyID);
                part->physicsBodyID = JPH::BodyID(); // Invalidate immediately on main thread
            }
        }
    }

    void PhysicsService::UnregisterPart(std::shared_ptr<BasePart> part) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (!part->physicsBodyID.IsInvalid()) {
            mPendingRemovals.push_back(part->physicsBodyID);
            part->physicsBodyID = JPH::BodyID();
        }
    }

    void PhysicsService::ProcessQueuedMutations() {
        // ASSUMPTION: mPhysicsMutex is already held by the caller (Physics Thread)
        std::vector<std::shared_ptr<BasePart>> toAdd;
        std::vector<JPH::BodyID> toRemove;

        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            toAdd.swap(mPendingRegisters);
            toRemove.swap(mPendingRemovals);
        }

        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();

        // 1. Process Removals
        if (!toRemove.empty()) {
            bi.RemoveBodies(toRemove.data(), (int)toRemove.size());
            bi.DestroyBodies(toRemove.data(), (int)toRemove.size());
            for (auto id : toRemove) bodyToPartMap.erase(id);
        }

        // 2. Process Additions
        JPH::BodyIDVector bodiesToAdd;
        for (auto& part : toAdd) {
            if (!part || !part->basePartProps) continue;
            
            auto& props = *part->basePartProps;
            auto cf = props.CFrame.get().to_nova();
            auto size = props.Size.get().to_glm();

            JPH::BoxShapeSettings shapeSettings(JPH::Vec3(std::max(0.05f, size.x * 0.5f), std::max(0.05f, size.y * 0.5f), std::max(0.05f, size.z * 0.5f)));
            JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
            if (!shapeResult.IsValid()) continue;

            bool anchored = props.Anchored;
            JPH::EMotionType motionType = anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
            JPH::ObjectLayer layer = anchored ? Layers::NON_MOVING : Layers::MOVING;

            glm::quat q = glm::normalize(glm::quat_cast(cf.rotation));
            JPH::BodyCreationSettings bodySettings(
                shapeResult.Get(),
                JPH::RVec3(cf.position.x, cf.position.y, cf.position.z),
                JPH::Quat(q.x, q.y, q.z, q.w),
                motionType,
                layer
            );

            bodySettings.mUserData = (uint64_t)part.get();
            if (motionType == JPH::EMotionType::Dynamic && (size.x * size.y * size.z) < 5.0f) {
                bodySettings.mMotionQuality = JPH::EMotionQuality::LinearCast;
            }

            JPH::Body* body = bi.CreateBody(bodySettings);
            if (body) {
                bodiesToAdd.push_back(body->GetID());
                part->physicsBodyID = body->GetID();
                part->registeredService = std::static_pointer_cast<PhysicsService>(shared_from_this());
                bodyToPartMap[body->GetID()] = part;
            }
        }

        if (!bodiesToAdd.empty()) {
            JPH::BodyInterface::AddState state = bi.AddBodiesPrepare(bodiesToAdd.data(), (int)bodiesToAdd.size());
            bi.AddBodiesFinalize(bodiesToAdd.data(), (int)bodiesToAdd.size(), state, JPH::EActivation::Activate);
            physicsSystem->OptimizeBroadPhase();
        }
    }

    void PhysicsService::Step(float dt) {
        std::vector<TransformUpdate> updates;
        {
            std::lock_guard<std::mutex> lock(mBufferMutex);
            updates.swap(mTransformBuffer);
        }

        float destroyHeight = -500.0f;
        auto dm = GetDataModel();
        std::shared_ptr<Workspace> ws = nullptr;
        if (dm) {
            ws = dm->GetService<Workspace>();
            if (ws) destroyHeight = ws->props.FallenPartsDestroyHeight;
        }

        std::vector<std::shared_ptr<BasePart>> toRemove;

        for (const auto& update : updates) {
            if (auto part = update.part.lock()) {
                // FallenPartsDestroyHeight Check
                if (update.position.y < destroyHeight) {
                    toRemove.push_back(part);
                    continue;
                }

                // Sync to C++ property so GetLocalTransform() returns the exact physical state
                if (part->basePartProps) {
                    auto& cf = part->basePartProps->CFrame;
                    auto nova_cf = cf.get().to_nova();
                    nova_cf.position = update.position;
                    nova_cf.rotation = glm::mat3_cast(update.rotation);
                    cf = CFrameReflect::from_nova(nova_cf);
                }
            }
        }

        // NON-BLOCKING Bulk removal
        if (!toRemove.empty()) {
            BulkUnregisterParts(toRemove);
            for (auto& part : toRemove) {
                if (auto p = part->parent.lock()) {
                    auto& c = p->children;
                    c.erase(std::remove(c.begin(), c.end(), part), c.end());
                }
                part->parent.reset();
            }
            if (ws) ws->RefreshCachedParts(); // Refresh once at the end
        }

        // Fire Touched signals
        std::vector<ContactEvent> contacts;
        {
            std::lock_guard<std::mutex> lock(mContactMutex);
            contacts.swap(mContactBuffer);
        }

        for (const auto& contact : contacts) {
            auto p1 = contact.part1.lock();
            auto p2 = contact.part2.lock();
            if (p1 && p2) {
                p1->Touched.fire(p2);
                p2->Touched.fire(p1);
            }
        }
    }

    void PhysicsService::SyncTransforms() {
        // ASSUMPTION: mPhysicsMutex is already held by the caller (Physics Thread)
        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();
        JPH::BodyIDVector activeBodies;

        physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);

        std::vector<TransformUpdate> updates;
        updates.reserve(activeBodies.size());

        for (const auto& id : activeBodies) {
            if (bi.GetMotionType(id) == JPH::EMotionType::Static) continue;

            // Safe lookup in the map (managed only on Physics Thread)
            auto it = bodyToPartMap.find(id);
            if (it == bodyToPartMap.end()) continue;

            if (auto part = it->second.lock()) {
                JPH::RVec3 pos;
                JPH::Quat rot;
                bi.GetPositionAndRotation(id, pos, rot);

                TransformUpdate update;
                update.part = part;
                update.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
                update.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
                updates.push_back(std::move(update));
            }
        }

        {
            std::lock_guard<std::mutex> lock(mBufferMutex);
            mTransformBuffer.insert(mTransformBuffer.end(), updates.begin(), updates.end());
        }
    }
}
