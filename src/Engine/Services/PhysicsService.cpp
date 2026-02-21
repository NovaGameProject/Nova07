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
#include "Engine/Physics/ContactListener.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <SDL3/SDL_log.h>
#include <cstdarg>
#include <thread>
#include <unordered_set>
#include <iostream>

namespace Nova {

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
        return true; 
    }
#endif

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
        physicsSystem->Init(262144, 2096, 262144, 262144, *bp_interface, *obp_filter, *olp_filter);
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
                    ProcessExplosions();  
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
        if (mThread.joinable()) mThread.join();
    }

    void PhysicsService::SetDeferRegistration(bool defer) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        if (mDeferring && !defer) {
            if (!mDeferredParts.empty()) {
                mPendingRegisters.insert(mPendingRegisters.end(), mDeferredParts.begin(), mDeferredParts.end());
                mDeferredParts.clear();
            }
        }
        mDeferring = defer;
    }

    void PhysicsService::BulkRegisterParts(const std::vector<std::shared_ptr<BasePart>>& parts) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        if (mDeferring) mDeferredParts.insert(mDeferredParts.end(), parts.begin(), parts.end());
        else mPendingRegisters.insert(mPendingRegisters.end(), parts.begin(), parts.end());
    }

    void PhysicsService::BulkUnregisterParts(const std::vector<BasePart*>& parts) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
        for (auto* part : parts) {
            mPartToJoints.erase(part);
            
            auto itAss = mPartToAssembly.find(part);
            if (itAss != mPartToAssembly.end()) {
                auto assembly = itAss->second;
                // Don't erase from assembly->parts here, UpdateAssemblies will handle it
                // because it's a vector of weak_ptrs now.
                
                // Find first alive part to trigger rebuild
                for (auto& wp : assembly->parts) {
                    if (auto alive = wp.lock()) {
                        if (alive.get() != part) {
                            mPendingAssemblyUpdates.push_back(alive);
                            break;
                        }
                    }
                }
            }

            if (!part->physicsBodyID.IsInvalid()) {
                uint64_t ptrVal = reinterpret_cast<uint64_t>(part);
                std::unique_lock<std::shared_mutex> jlock(mJoinedPairsMutex);
                std::erase_if(mJoinedPairs, [ptrVal](const PartPair& p) {
                    return p.first == ptrVal || p.second == ptrVal;
                });
                part->physicsBodyID = JPH::BodyID();
            }
            mPartToAssembly.erase(part);
        }
    }

    void PhysicsService::UnregisterPart(BasePart* part) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
        mPartToJoints.erase(part);

        auto itAss = mPartToAssembly.find(part);
        if (itAss != mPartToAssembly.end()) {
            auto assembly = itAss->second;
            for (auto& wp : assembly->parts) {
                if (auto alive = wp.lock()) {
                    if (alive.get() != part) {
                        mPendingAssemblyUpdates.push_back(alive);
                        break;
                    }
                }
            }
        }

        if (!part->physicsBodyID.IsInvalid()) {
            uint64_t ptrVal = reinterpret_cast<uint64_t>(part);
            std::unique_lock<std::shared_mutex> jlock(mJoinedPairsMutex);
            std::erase_if(mJoinedPairs, [ptrVal](const PartPair& p) {
                return p.first == ptrVal || p.second == ptrVal;
            });
            part->physicsBodyID = JPH::BodyID();
        }
        mPartToAssembly.erase(part);
    }

    void PhysicsService::RegisterConstraint(JointInstance* joint) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        mPendingConstraints.push_back(std::static_pointer_cast<JointInstance>(joint->shared_from_this()));
    }

    void PhysicsService::UnregisterConstraint(JointInstance* joint) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);

        auto p0 = joint->Part0.lock();
        auto p1 = joint->Part1.lock();
        if (p0) {
            auto& joints = mPartToJoints[p0.get()];
            joints.erase(std::remove_if(joints.begin(), joints.end(), [joint](const std::weak_ptr<JointInstance>& w) {
                auto s = w.lock();
                return !s || s.get() == joint;
            }), joints.end());
            mPendingAssemblyUpdates.push_back(p0);
        }
        if (p1) {
            auto& joints = mPartToJoints[p1.get()];
            joints.erase(std::remove_if(joints.begin(), joints.end(), [joint](const std::weak_ptr<JointInstance>& w) {
                auto s = w.lock();
                return !s || s.get() == joint;
            }), joints.end());
            mPendingAssemblyUpdates.push_back(p1);
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
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        PartPair pair = { reinterpret_cast<uint64_t>(p1), reinterpret_cast<uint64_t>(p2) };
        if (pair.first > pair.second) std::swap(pair.first, pair.second);
        return mJoinedPairs.contains(pair);
    }

    void PhysicsService::RequestAssemblyUpdate(BasePart* part) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        mPendingAssemblyUpdates.push_back(std::static_pointer_cast<BasePart>(part->shared_from_this()));
    }

    void PhysicsService::BreakJoints(BasePart* part) {
        if (!part) return;
        
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);

        std::cout << "[BreakJoints] Breaking joints for part: " << part->GetName() << std::endl;

        auto it = mPartToJoints.find(part);
        if (it != mPartToJoints.end()) {
            auto joints = it->second;
            it->second.clear();
            
            for (auto& weakJoint : joints) {
                if (auto joint = weakJoint.lock()) {
                    auto p0 = joint->Part0.lock();
                    auto p1 = joint->Part1.lock();
                    
                    BasePart* other = (p0.get() == part) ? p1.get() : p0.get();
                    if (other) {
                        auto itOther = mPartToJoints.find(other);
                        if (itOther != mPartToJoints.end()) {
                            auto& otherJoints = itOther->second;
                            otherJoints.erase(std::remove_if(otherJoints.begin(), otherJoints.end(),
                                [&](auto& w) { return w.lock() == joint; }), otherJoints.end());
                        }
                        mPendingAssemblyUpdates.push_back(std::static_pointer_cast<BasePart>(other->shared_from_this()));
                    }

                    if (joint->physicsConstraint) {
                        mPendingConstraintRemovals.push_back(joint->physicsConstraint);
                        joint->physicsConstraint = nullptr;
                    }
                    
                    mPendingJointDestructions.push_back(joint);
                }
            }
        }

        auto it2 = mPartToAutoJoints.find(part);
        if (it2 != mPartToAutoJoints.end()) {
            auto reqs = it2->second;
            it2->second.clear();
            
            for (auto& req : reqs) {
                auto p0 = req->part1.lock();
                auto p1 = req->part2.lock();
                
                BasePart* other = (p0.get() == part) ? p1.get() : p0.get();
                if (other) {
                    auto itOther2 = mPartToAutoJoints.find(other);
                    if (itOther2 != mPartToAutoJoints.end()) {
                        auto& otherReqs = itOther2->second;
                        otherReqs.erase(std::remove(otherReqs.begin(), otherReqs.end(), req), otherReqs.end());
                    }
                    mPendingAssemblyUpdates.push_back(std::static_pointer_cast<BasePart>(other->shared_from_this()));
                }
                
                mInternalJointsToRemove.push_back(req);
                
                if (p0 && p1) {
                    PartPair pair = { reinterpret_cast<uint64_t>(p0.get()), reinterpret_cast<uint64_t>(p1.get()) };
                    if (pair.first > pair.second) std::swap(pair.first, pair.second);
                    std::unique_lock<std::shared_mutex> lock_pairs(mJoinedPairsMutex);
                    mJoinedPairs.erase(pair);
                }
            }
        }

        mPendingAssemblyUpdates.push_back(std::static_pointer_cast<BasePart>(part->shared_from_this()));
    }

    void PhysicsService::QueueExplosion(glm::vec3 position, float radius, float pressure) {
        std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
        mPendingExplosions.push_back({position, radius, pressure});
    }

    void PhysicsService::BreakJointsInRadius(glm::vec3 position, float radius) {
        JPH::SphereShape sphere(radius);
        JPH::CollideShapeSettings settings;
        JointBreakCollector collector(this);
        std::lock_guard<std::recursive_mutex> lock(mPhysicsMutex);
        physicsSystem->GetNarrowPhaseQuery().CollideShape(
            &sphere, JPH::Vec3::sReplicate(1.0f),
            JPH::RMat44::sTranslation(JPH::RVec3(position.x, position.y, position.z)),
            settings, JPH::RVec3::sZero(), collector
        );
    }

    void PhysicsService::Step(float dt) {
        std::vector<TransformUpdate> updates;
        {
            std::lock_guard<std::mutex> lock(mBufferMutex);
            updates.swap(mTransformBuffer);
        }

        std::vector<std::shared_ptr<JointInstance>> toDestroy;
        {
            std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
            toDestroy.swap(mPendingJointDestructions);
        }
        for (auto& joint : toDestroy) {
            joint->SetParent(nullptr);
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
            std::vector<BasePart*> rawParts;
            for (auto& p : toRemove) rawParts.push_back(p.get());
            BulkUnregisterParts(rawParts);
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
        std::shared_lock<std::shared_mutex> mapLock(mMapsMutex);
        for (const auto& id : activeBodies) {
            if (bi.GetMotionType(id) == JPH::EMotionType::Static) continue;
            auto it = mBodyToAssembly.find(id);
            if (it == mBodyToAssembly.end()) continue;
            auto assembly = it->second;
            JPH::RVec3 pos;
            JPH::Quat rot;
            bi.GetPositionAndRotation(id, pos, rot);
            CFrame bodyCF;
            bodyCF.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            bodyCF.rotation = glm::mat3_cast(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));
            for (auto& wp : assembly->parts) {
                auto part = wp.lock();
                if (!part) continue;
                auto itRel = assembly->relativeTransforms.find(part.get());
                if (itRel == assembly->relativeTransforms.end()) continue;
                CFrame world = bodyCF * itRel->second;
                TransformUpdate update;
                update.part = part;
                update.position = world.position;
                update.rotation = glm::quat_cast(world.rotation);
                updates.push_back(std::move(update));
            }
        }
        {
            std::lock_guard<std::mutex> lock(mBufferMutex);
            mTransformBuffer.insert(mTransformBuffer.end(), updates.begin(), updates.end());
        }
    }
}
