// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "PhysicsService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/JointInstance.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/DataModel.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <SDL3/SDL_log.h>
#include <cstdarg>
#include <thread>
#include <unordered_set>

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
                    return inBroadPhaseLayer == BroadPhaseLayers::NON_MOVING || inBroadPhaseLayer == BroadPhaseLayers::MOVING;
                default: return false;
            }
        }
    };

    static bool AreSurfacesCompatible(SurfaceType s1, SurfaceType s2) {
        if (s1 == SurfaceType::Weld || s2 == SurfaceType::Weld) return true;
        if (s1 == SurfaceType::Glue || s2 == SurfaceType::Glue) return true;

        if (s1 == SurfaceType::Studs && (s2 == SurfaceType::Inlets || s2 == SurfaceType::Universal)) return true;
        if (s1 == SurfaceType::Inlets && (s2 == SurfaceType::Studs || s2 == SurfaceType::Universal)) return true;
        if (s1 == SurfaceType::Universal && (s2 == SurfaceType::Studs || s2 == SurfaceType::Inlets || s2 == SurfaceType::Universal)) return true;

        return false;
    }

    static bool IsAligned(glm::vec3 localNormal) {
        float ax = std::abs(localNormal.x);
        float ay = std::abs(localNormal.y);
        float az = std::abs(localNormal.z);
        return (ax > 0.95f && ay < 0.05f && az < 0.05f) ||
               (ay > 0.95f && ax < 0.05f && az < 0.05f) ||
               (az > 0.95f && ax < 0.05f && ay < 0.05f);
    }

    class PhysicsService::ContactListenerImpl : public JPH::ContactListener {
    public:
        PhysicsService* service;
        ContactListenerImpl(PhysicsService* service) : service(service) {}

        JPH::ValidateResult OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override {
            auto id1 = inBody1.GetID();
            auto id2 = inBody2.GetID();

            // Use shared_lock for high-performance concurrent reads
            std::shared_lock<std::shared_mutex> lock(service->mJoinedPairsMutex);

            auto it1 = service->bodyToPartMap.find(id1);
            auto it2 = service->bodyToPartMap.find(id2);

            if (it1 != service->bodyToPartMap.end() && it2 != service->bodyToPartMap.end()) {
                 auto p1 = it1->second.lock();
                 auto p2 = it2->second.lock();
                 if (p1 && p2) {
                     PhysicsService::PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
                     if (pair.first > pair.second) std::swap(pair.first, pair.second);

                     if (service->mJoinedPairs.contains(pair)) {
                         return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
                     }
                 }
            }
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override {
            auto id1 = inBody1.GetID();
            auto id2 = inBody2.GetID();

            auto it1 = service->bodyToPartMap.find(id1);
            auto it2 = service->bodyToPartMap.find(id2);

            if (it1 != service->bodyToPartMap.end() && it2 != service->bodyToPartMap.end()) {
                auto p1 = it1->second.lock();
                auto p2 = it2->second.lock();

                if (p1 && p2) {
                    {
                        std::lock_guard<std::mutex> lock(service->mContactMutex);
                        service->mContactBuffer.push_back({ p1, p2 });
                    }

                    JPH::Vec3 worldNormal = inManifold.mWorldSpaceNormal;
                    JPH::RMat44 invM1 = inBody1.GetInverseCenterOfMassTransform();
                    JPH::RMat44 invM2 = inBody2.GetInverseCenterOfMassTransform();

                    JPH::Vec3 localN1 = invM1.Multiply3x3(worldNormal);
                    JPH::Vec3 localN2 = invM2.Multiply3x3(-worldNormal);

                    SurfaceType s1 = p1->GetSurfaceType(glm::vec3(localN1.GetX(), localN1.GetY(), localN1.GetZ()));
                    SurfaceType s2 = p2->GetSurfaceType(glm::vec3(localN2.GetX(), localN2.GetY(), localN2.GetZ()));

                    // Requirement: Compatible surfaces, aligned normals, AND "Full Contact" (at least 4 points)
                    if (AreSurfacesCompatible(s1, s2) && 
                        IsAligned(glm::vec3(localN1.GetX(), localN1.GetY(), localN1.GetZ())) && 
                        IsAligned(glm::vec3(localN2.GetX(), localN2.GetY(), localN2.GetZ())) &&
                        inManifold.mRelativeContactPointsOn1.size() >= 4) 
                    {
                        PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
                        if (pair.first > pair.second) std::swap(pair.first, pair.second);

                        bool alreadyJoined = false;
                        {
                            std::shared_lock<std::shared_mutex> lock(service->mJoinedPairsMutex);
                            alreadyJoined = service->mJoinedPairs.contains(pair);
                        }

                        if (!alreadyJoined) {
                            std::lock_guard<std::mutex> lock(service->mQueueMutex);
                            service->mPendingAutoJoints.push_back({ p1, p2, s1, s2 });
                        }
                    }
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

        tempAllocator = new JPH::TempAllocatorImpl(256 * 1024 * 1024);
        jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::max(1, (int)std::thread::hardware_concurrency() - 1));

        bp_interface = std::make_unique<BPLInterfaceImpl>();
        obp_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
        olp_filter = std::make_unique<ObjectLayerPairFilterImpl>();
        contact_listener = std::make_unique<ContactListenerImpl>(this);

        physicsSystem = new JPH::PhysicsSystem();
        physicsSystem->Init(
            262144, // Max Bodies
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
        settings.mPointVelocitySleepThreshold = 1.0f;
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
                    ProcessQueuedMutations();
                    UpdateAssemblies();
                    float internalDt = std::min(dt, 1.0f / 60.0f);
                    physicsSystem->Update(internalDt, 1, tempAllocator, jobSystem);
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
            mPartToJoints.erase(part.get());
            if (!part->physicsBodyID.IsInvalid()) {
                mPendingRemovals.push_back(part->physicsBodyID);
                part->physicsBodyID = JPH::BodyID();

                uint64_t ptrVal = reinterpret_cast<uint64_t>(part.get());
                std::unique_lock<std::shared_mutex> jlock(mJoinedPairsMutex);
                std::erase_if(mJoinedPairs, [ptrVal](const PartPair& p) {
                    return p.first == ptrVal || p.second == ptrVal;
                });
            }
        }
    }

    void PhysicsService::UnregisterPart(std::shared_ptr<BasePart> part) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mPartToJoints.erase(part.get());
        if (!part->physicsBodyID.IsInvalid()) {
            mPendingRemovals.push_back(part->physicsBodyID);
            part->physicsBodyID = JPH::BodyID();

            uint64_t ptrVal = reinterpret_cast<uint64_t>(part.get());
            std::unique_lock<std::shared_mutex> jlock(mJoinedPairsMutex);
            std::erase_if(mJoinedPairs, [ptrVal](const PartPair& p) {
                return p.first == ptrVal || p.second == ptrVal;
            });
        }
    }

    void PhysicsService::RegisterConstraint(JointInstance* joint) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mPendingConstraints.push_back(std::static_pointer_cast<JointInstance>(joint->shared_from_this()));
    }

    void PhysicsService::UnregisterConstraint(JointInstance* joint) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        
        auto p0 = joint->Part0.lock();
        auto p1 = joint->Part1.lock();
        
        if (p0) {
            auto& joints = mPartToJoints[p0.get()];
            joints.erase(std::remove_if(joints.begin(), joints.end(), [joint](const std::weak_ptr<JointInstance>& w) {
                auto s = w.lock();
                return !s || s.get() == joint;
            }), joints.end());
            mPendingAssemblyUpdates.push_back(p0.get());
        }
        if (p1) {
            auto& joints = mPartToJoints[p1.get()];
            joints.erase(std::remove_if(joints.begin(), joints.end(), [joint](const std::weak_ptr<JointInstance>& w) {
                auto s = w.lock();
                return !s || s.get() == joint;
            }), joints.end());
            mPendingAssemblyUpdates.push_back(p1.get());
        }

        if (joint->physicsConstraint) {
            mPendingConstraintRemovals.push_back(joint->physicsConstraint);
            joint->physicsConstraint = nullptr;

            if (p0 && p1) {
                PartPair pair = { reinterpret_cast<uint64_t>(p0.get()), reinterpret_cast<uint64_t>(p1.get()) };
                if (pair.first > pair.second) std::swap(pair.first, pair.second);
                std::unique_lock<std::shared_mutex> lock_pairs(mJoinedPairsMutex);
                mJoinedPairs.erase(pair);
            }
        }
    }

    bool PhysicsService::HasJointBetween(BasePart* p1, BasePart* p2) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        PartPair pair = { reinterpret_cast<uint64_t>(p1), reinterpret_cast<uint64_t>(p2) };
        if (pair.first > pair.second) std::swap(pair.first, pair.second);
        return mJoinedPairs.contains(pair);
    }

    void PhysicsService::RequestAssemblyUpdate(BasePart* part) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mPendingAssemblyUpdates.push_back(part);
    }

    void PhysicsService::BreakJoints(BasePart* part) {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        
        // 1. Break JointInstances (Instances in the tree)
        auto it = mPartToJoints.find(part);
        if (it != mPartToJoints.end()) {
            for (auto& weakJoint : it->second) {
                if (auto joint = weakJoint.lock()) {
                    if (joint->physicsConstraint) {
                        mPendingConstraintRemovals.push_back(joint->physicsConstraint);
                        joint->physicsConstraint = nullptr;
                    }
                }
            }
        }

        // 2. Break AutoJoints (Internal connections)
        auto it2 = mPartToAutoJoints.find(part);
        if (it2 != mPartToAutoJoints.end()) {
            for (auto& req : it2->second) {
                if (req->physicsConstraint) {
                    mInternalJointsToRemove.push_back(req->physicsConstraint);
                    req->physicsConstraint = nullptr;
                }
            }
        }
    }

    class JointBreakCollector : public JPH::CollideShapeCollector {
    public:
        PhysicsService* service;
        JointBreakCollector(PhysicsService* s) : service(s) {}

        void AddHit(const JPH::CollideShapeResult &inResult) override {
            auto it = service->bodyToPartMap.find(inResult.mBodyID2);
            if (it != service->bodyToPartMap.end()) {
                if (auto part = it->second.lock()) {
                    service->BreakJoints(part.get());
                }
            }
        }
    };

    void PhysicsService::BreakJointsInRadius(glm::vec3 position, float radius) {
        JPH::SphereShape sphere(radius);
        JPH::CollideShapeSettings settings;
        JointBreakCollector collector(this);
        
        // We need to lock physics system for query
        std::lock_guard<std::recursive_mutex> lock(mPhysicsMutex);
        physicsSystem->GetNarrowPhaseQuery().CollideShape(
            &sphere, JPH::Vec3::sReplicate(1.0f), 
            JPH::RMat44::sTranslation(JPH::RVec3(position.x, position.y, position.z)),
            settings, JPH::RVec3::sZero(), collector
        );
    }

    void PhysicsService::UpdateAssemblies() {
        std::vector<BasePart*> updates;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            updates.swap(mPendingAssemblyUpdates);
        }

        if (updates.empty()) return;

        // Flood fill to determine static state
        std::unordered_set<BasePart*> visited;
        std::unordered_set<BasePart*> anchoredParts;

        // 1. Identify all parts in the affected assemblies
        std::vector<BasePart*> stack = updates;
        std::vector<BasePart*> allInAssemblies;
        
        while (!stack.empty()) {
            BasePart* p = stack.back();
            stack.pop_back();
            if (!visited.insert(p).second) continue;
            allInAssemblies.push_back(p);

            if (p->basePartProps && p->basePartProps->Anchored) {
                anchoredParts.insert(p);
            }

            auto it = mPartToJoints.find(p);
            if (it != mPartToJoints.end()) {
                for (auto& weakJoint : it->second) {
                    if (auto joint = weakJoint.lock()) {
                        if (joint->GetClassName() == "Motor") continue;

                        auto p0 = joint->Part0.lock();
                        auto p1 = joint->Part1.lock();
                        BasePart* other = (p0.get() == p) ? p1.get() : p0.get();
                        if (other && !visited.contains(other)) {
                            stack.push_back(other);
                        }
                    }
                }
            }

            auto it2 = mPartToAutoJoints.find(p);
            if (it2 != mPartToAutoJoints.end()) {
                for (auto& req : it2->second) {
                    auto p0 = req->part1.lock();
                    auto p1 = req->part2.lock();
                    BasePart* other = (p0.get() == p) ? p1.get() : p0.get();
                    if (other && !visited.contains(other)) {
                        stack.push_back(other);
                    }
                }
            }
        }

        // 2. Flood fill from anchored parts to mark static
        std::unordered_set<BasePart*> shouldBeStatic;
        std::vector<BasePart*> staticStack;
        for (auto p : anchoredParts) staticStack.push_back(p);

        while (!staticStack.empty()) {
            BasePart* p = staticStack.back();
            staticStack.pop_back();
            if (!shouldBeStatic.insert(p).second) continue;

            auto it = mPartToJoints.find(p);
            if (it != mPartToJoints.end()) {
                for (auto& weakJoint : it->second) {
                    if (auto joint = weakJoint.lock()) {
                        if (joint->GetClassName() == "Motor") continue;

                        auto p0 = joint->Part0.lock();
                        auto p1 = joint->Part1.lock();
                        BasePart* other = (p0.get() == p) ? p1.get() : p0.get();
                        if (other && !shouldBeStatic.contains(other)) {
                            staticStack.push_back(other);
                        }
                    }
                }
            }

            auto it2 = mPartToAutoJoints.find(p);
            if (it2 != mPartToAutoJoints.end()) {
                for (auto& req : it2->second) {
                    auto p0 = req->part1.lock();
                    auto p1 = req->part2.lock();
                    BasePart* other = (p0.get() == p) ? p1.get() : p0.get();
                    if (other && !shouldBeStatic.contains(other)) {
                        staticStack.push_back(other);
                    }
                }
            }
        }

        // 3. Apply states to Jolt bodies
        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();
        bool changed = false;
        for (auto p : allInAssemblies) {
            if (p->physicsBodyID.IsInvalid()) continue;

            bool isStatic = shouldBeStatic.contains(p);
            JPH::EMotionType currentMotion = bi.GetMotionType(p->physicsBodyID);
            
            if (isStatic && currentMotion != JPH::EMotionType::Static) {
                bi.SetLinearVelocity(p->physicsBodyID, JPH::Vec3::sZero());
                bi.SetAngularVelocity(p->physicsBodyID, JPH::Vec3::sZero());
                bi.SetMotionType(p->physicsBodyID, JPH::EMotionType::Static, JPH::EActivation::DontActivate);
                bi.SetObjectLayer(p->physicsBodyID, Layers::NON_MOVING);
                changed = true;
            } else if (!isStatic && currentMotion == JPH::EMotionType::Static) {
                // If it was static but now should be dynamic (unless it's actually anchored)
                if (p->basePartProps && !p->basePartProps->Anchored) {
                    bi.SetMotionType(p->physicsBodyID, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
                    bi.SetObjectLayer(p->physicsBodyID, Layers::MOVING);
                    changed = true;
                }
            }
        }

        if (changed) {
            physicsSystem->OptimizeBroadPhase();
        }
    }

    void PhysicsService::ProcessQueuedMutations() {
        std::vector<std::shared_ptr<BasePart>> toAdd;
        std::vector<JPH::BodyID> toRemove;
        std::vector<std::shared_ptr<JointInstance>> constraintsToAdd;
        std::vector<JPH::Constraint*> constraintsToRemove;
        std::vector<JointRequest> autoJoints;

        std::vector<JPH::Constraint*> internalRemovals;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            toAdd.swap(mPendingRegisters);
            toRemove.swap(mPendingRemovals);
            constraintsToAdd.swap(mPendingConstraints);
            constraintsToRemove.swap(mPendingConstraintRemovals);
            autoJoints.swap(mPendingAutoJoints);
            internalRemovals.swap(mInternalJointsToRemove);
        }

        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();

        for (auto c : constraintsToRemove) {
            physicsSystem->RemoveConstraint(c);
        }

        for (auto c : internalRemovals) {
            physicsSystem->RemoveConstraint(c);
            // We need to find and remove it from active lists
            auto it = std::find_if(mActiveAutoJoints.begin(), mActiveAutoJoints.end(), [c](const std::shared_ptr<InternalJoint>& r) {
                return r->physicsConstraint == c;
            });
            if (it != mActiveAutoJoints.end()) {
                auto p0 = (*it)->part1.lock();
                auto p1 = (*it)->part2.lock();
                if (p0) {
                    auto& v = mPartToAutoJoints[p0.get()];
                    v.erase(std::remove(v.begin(), v.end(), *it), v.end());
                    mPendingAssemblyUpdates.push_back(p0.get());
                }
                if (p1) {
                    auto& v = mPartToAutoJoints[p1.get()];
                    v.erase(std::remove(v.begin(), v.end(), *it), v.end());
                    mPendingAssemblyUpdates.push_back(p1.get());
                }
                mActiveAutoJoints.erase(it);
            }
        }

        if (!toRemove.empty()) {
            bi.RemoveBodies(toRemove.data(), (int)toRemove.size());
            bi.DestroyBodies(toRemove.data(), (int)toRemove.size());
            for (auto id : toRemove) bodyToPartMap.erase(id);
        }

        JPH::BodyIDVector bodiesToAdd;
        for (auto& part : toAdd) {
            if (!part || !part->basePartProps) continue;

            auto& props = *part->basePartProps;
            auto cf = props.CFrame.get().to_nova();
            auto size = props.Size.get().to_glm();

            JPH::BoxShapeSettings shapeSettings(JPH::Vec3(std::max(0.05f, size.x * 0.5f), std::max(0.05f, size.y * 0.5f), std::max(0.05f, size.z * 0.5f)));
            shapeSettings.mDensity = 1.0f; // Roblox-like density (prevents extreme joint stress)
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
            bodySettings.mMaxLinearVelocity = 1000.0f; // Prevent NaN explosion
            if (motionType == JPH::EMotionType::Dynamic && (size.x * size.y * size.z) < 5.0f) {
                bodySettings.mMotionQuality = JPH::EMotionQuality::LinearCast;
            }

            JPH::Body* body = bi.CreateBody(bodySettings);
            if (body) {
                bodiesToAdd.push_back(body->GetID());
                part->physicsBodyID = body->GetID();
                part->registeredService = std::static_pointer_cast<PhysicsService>(shared_from_this());
                bodyToPartMap[body->GetID()] = part;

                if (anchored) mPendingAssemblyUpdates.push_back(part.get());
            }
        }

        if (!bodiesToAdd.empty()) {
            JPH::BodyInterface::AddState state = bi.AddBodiesPrepare(bodiesToAdd.data(), (int)bodiesToAdd.size());
            bi.AddBodiesFinalize(bodiesToAdd.data(), (int)bodiesToAdd.size(), state, JPH::EActivation::Activate);
            physicsSystem->OptimizeBroadPhase();
        }

        for (auto joint : constraintsToAdd) {
            auto p0 = joint->Part0.lock();
            auto p1 = joint->Part1.lock();
            if (!p0 || !p1 || p0->physicsBodyID.IsInvalid() || p1->physicsBodyID.IsInvalid()) continue;

            PartPair pair = { reinterpret_cast<uint64_t>(p0.get()), reinterpret_cast<uint64_t>(p1.get()) };
            if (pair.first > pair.second) std::swap(pair.first, pair.second);

            {
                std::lock_guard<std::mutex> lock(mQueueMutex);
                if (mJoinedPairs.contains(pair)) continue;
            }

            JPH::BodyID ids[] = { p0->physicsBodyID, p1->physicsBodyID };
            JPH::Constraint* c = nullptr;

            {
                JPH::BodyLockMultiWrite multiLock(physicsSystem->GetBodyLockInterface(), ids, 2);
                if (multiLock.GetBody(0) && multiLock.GetBody(1)) {
                    if (joint->GetClassName() == "Weld" || joint->GetClassName() == "Snap" || joint->GetClassName() == "Glue" || joint->GetClassName() == "AutoJoint") {
                        JPH::FixedConstraintSettings settings;
                        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;

                        CFrame cf0, cf1;
                        if (joint->GetClassName() == "AutoJoint") {
                            auto* aj = static_cast<AutoJoint*>(joint.get());
                            cf0 = aj->props.C0.get().to_nova();
                            cf1 = aj->props.C1.get().to_nova();
                        } else {
                            auto* weld = static_cast<Weld*>(joint.get());
                            cf0 = weld->props.C0.get().to_nova();
                            cf1 = weld->props.C1.get().to_nova();
                        }

                        settings.mPoint1 = JPH::RVec3(cf0.position.x, cf0.position.y, cf0.position.z);
                        settings.mPoint2 = JPH::RVec3(cf1.position.x, cf1.position.y, cf1.position.z);
                        settings.mAxisX1 = JPH::Vec3(cf0.rotation[0].x, cf0.rotation[0].y, cf0.rotation[0].z);
                        settings.mAxisY1 = JPH::Vec3(cf0.rotation[1].x, cf0.rotation[1].y, cf0.rotation[1].z);
                        settings.mAxisX2 = JPH::Vec3(cf1.rotation[0].x, cf1.rotation[0].y, cf1.rotation[0].z);
                        settings.mAxisY2 = JPH::Vec3(cf1.rotation[1].x, cf1.rotation[1].y, cf1.rotation[1].z);

                        c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                    }
                    else if (joint->GetClassName() == "Motor") {
                        JPH::HingeConstraintSettings settings;
                        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                        auto cf0 = static_cast<Motor*>(joint.get())->props.base.get().C0.get().to_nova();
                        auto cf1 = static_cast<Motor*>(joint.get())->props.base.get().C1.get().to_nova();
                        settings.mPoint1 = JPH::RVec3(cf0.position.x, cf0.position.y, cf0.position.z);
                        settings.mPoint2 = JPH::RVec3(cf1.position.x, cf1.position.y, cf1.position.z);
                        settings.mHingeAxis1 = JPH::Vec3(cf0.rotation[0].x, cf0.rotation[0].y, cf0.rotation[0].z);
                        settings.mHingeAxis2 = JPH::Vec3(cf1.rotation[0].x, cf1.rotation[0].y, cf1.rotation[0].z);
                        settings.mNormalAxis1 = JPH::Vec3(cf0.rotation[1].x, cf0.rotation[1].y, cf0.rotation[1].z);
                        settings.mNormalAxis2 = JPH::Vec3(cf1.rotation[1].x, cf1.rotation[1].y, cf1.rotation[1].z);
                        settings.mMotorSettings.mSpringSettings.mFrequency = 2.0f;
                        settings.mMotorSettings.mSpringSettings.mDamping = 1.0f;
                        c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                    }
                }
            }

            if (c) {
                physicsSystem->AddConstraint(c);
                joint->physicsConstraint = c;
                joint->registeredService = std::static_pointer_cast<PhysicsService>(shared_from_this());
                
                std::weak_ptr<JointInstance> weakJoint = joint;
                mPartToJoints[p0.get()].push_back(weakJoint);
                mPartToJoints[p1.get()].push_back(weakJoint);
                mPendingAssemblyUpdates.push_back(p0.get());
                mPendingAssemblyUpdates.push_back(p1.get());

                {
                    std::unique_lock<std::shared_mutex> lock(mJoinedPairsMutex);
                    mJoinedPairs.insert(pair);
                }
            }
        }

        // 5. Process AutoJoint requests
        for (auto& req : autoJoints) {
            auto p1 = req.part1.lock();
            auto p2 = req.part2.lock();
            if (!p1 || !p2 || p1->physicsBodyID.IsInvalid() || p2->physicsBodyID.IsInvalid()) continue;

            PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
            if (pair.first > pair.second) std::swap(pair.first, pair.second);

            {
                std::shared_lock<std::shared_mutex> lock(mJoinedPairsMutex);
                if (mJoinedPairs.contains(pair)) continue;
            }

            JPH::BodyID ids[] = { p1->physicsBodyID, p2->physicsBodyID };
            JPH::Constraint* c = nullptr;
            {
                JPH::BodyLockMultiWrite multiLock(physicsSystem->GetBodyLockInterface(), ids, 2);
                if (multiLock.GetBody(0) && multiLock.GetBody(1)) {
                    JPH::FixedConstraintSettings settings;
                    settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;

                    JPH::RMat44 invTransform2 = multiLock.GetBody(1)->GetInverseCenterOfMassTransform();
                    JPH::RMat44 transform1 = multiLock.GetBody(0)->GetCenterOfMassTransform();
                    JPH::RMat44 relTransform = invTransform2 * transform1;

                    settings.mPoint1 = JPH::RVec3(0, 0, 0);
                    settings.mPoint2 = relTransform.GetTranslation();
                    settings.mAxisX1 = JPH::Vec3(1, 0, 0);
                    settings.mAxisY1 = JPH::Vec3(0, 1, 0);
                    settings.mAxisX2 = relTransform.GetAxisX();
                    settings.mAxisY2 = relTransform.GetAxisY();

                    c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                }
            }

            if (c) {
                physicsSystem->AddConstraint(c);
                
                auto activeReq = std::make_shared<InternalJoint>();
                activeReq->part1 = req.part1;
                activeReq->part2 = req.part2;
                activeReq->physicsConstraint = c;
                
                mActiveAutoJoints.push_back(activeReq);
                
                mPartToAutoJoints[p1.get()].push_back(activeReq);
                mPartToAutoJoints[p2.get()].push_back(activeReq);
                mPendingAssemblyUpdates.push_back(p1.get());
                mPendingAssemblyUpdates.push_back(p2.get());

                {
                    std::unique_lock<std::shared_mutex> lock(mJoinedPairsMutex);
                    mJoinedPairs.insert(pair);
                }
            }
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
                if (update.position.y < destroyHeight) {
                    toRemove.push_back(part);
                    continue;
                }

                if (part->basePartProps) {
                    auto& cf = part->basePartProps->CFrame;
                    auto nova_cf = cf.get().to_nova();
                    nova_cf.position = update.position;
                    nova_cf.rotation = glm::mat3_cast(update.rotation);
                    cf = CFrameReflect::from_nova(nova_cf);
                }
            }
        }

        if (!toRemove.empty()) {
            BulkUnregisterParts(toRemove);
            for (auto& part : toRemove) {
                if (auto p = part->parent.lock()) {
                    auto& c = p->children;
                    c.erase(std::remove(c.begin(), c.end(), part), c.end());
                }
                part->parent.reset();
            }
            if (ws) ws->RefreshCachedParts();
        }

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
        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();
        JPH::BodyIDVector activeBodies;

        physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);

        std::vector<TransformUpdate> updates;
        updates.reserve(activeBodies.size());

        for (const auto& id : activeBodies) {
            if (bi.GetMotionType(id) == JPH::EMotionType::Static) continue;

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
