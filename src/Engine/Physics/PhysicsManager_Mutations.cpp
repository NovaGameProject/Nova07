// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/JointInstance.hpp"
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <iostream>

namespace Nova {

    void PhysicsService::ProcessQueuedMutations() {
        std::vector<std::shared_ptr<BasePart>> toAdd;
        std::vector<JPH::BodyID> toRemove;
        std::vector<std::shared_ptr<JointInstance>> constraintsToAdd;
        std::vector<JPH::Constraint*> constraintsToRemove;
        std::vector<JointRequest> autoJoints;
        std::vector<std::shared_ptr<InternalJoint>> internalRemovals;
        {
            std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
            toAdd.swap(mPendingRegisters);
            toRemove.swap(mPendingRemovals);
            constraintsToAdd.swap(mPendingConstraints);
            constraintsToRemove.swap(mPendingConstraintRemovals);
            autoJoints.swap(mPendingAutoJoints);
            internalRemovals.swap(mInternalJointsToRemove);
        }

        JPH::BodyInterface &bi = physicsSystem->GetBodyInterface();

        {
            std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
            for (auto* constraint : constraintsToRemove) {
                if (!constraint) continue;
                for (auto& [id, ass] : mBodyToAssembly) {
                    if (ass->attachedConstraints.contains(constraint)) {
                        for (auto& wp : ass->parts) {
                            if (auto p = wp.lock()) {
                                auto itJ = mPartToJoints.find(p.get());
                                if (itJ != mPartToJoints.end()) {
                                    for (auto& weakJoint : itJ->second) {
                                        if (auto joint = weakJoint.lock()) {
                                            if (joint->physicsConstraint == constraint) {
                                                joint->physicsConstraint = nullptr;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        ass->attachedConstraints.erase(constraint);
                    }
                }
                physicsSystem->RemoveConstraint(constraint);
            }
        }

        for (auto& joint : internalRemovals) {
            if (joint->physicsConstraint) {
                physicsSystem->RemoveConstraint(joint->physicsConstraint);
                joint->physicsConstraint = nullptr;
            }
            auto it = std::find(mActiveAutoJoints.begin(), mActiveAutoJoints.end(), joint);
            if (it != mActiveAutoJoints.end()) {
                auto p0 = joint->part1.lock();
                auto p1 = joint->part2.lock();
                if (p0) {
                    auto& v = mPartToAutoJoints[p0.get()];
                    v.erase(std::remove(v.begin(), v.end(), joint), v.end());
                    mPendingAssemblyUpdates.push_back(p0);
                }
                if (p1) {
                    auto& v = mPartToAutoJoints[p1.get()];
                    v.erase(std::remove(v.begin(), v.end(), joint), v.end());
                    mPendingAssemblyUpdates.push_back(p1);
                }
                mActiveAutoJoints.erase(it);
            }
        }

        if (!toRemove.empty()) {
            std::vector<JPH::BodyID> actualRemovals;
            {
                std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
                for (auto id : toRemove) {
                    if (mAllActiveBodies.contains(id) && mBodyToAssembly.find(id) == mBodyToAssembly.end()) {
                        actualRemovals.push_back(id);
                        mAllActiveBodies.erase(id);
                    }
                }
            }

            if (!actualRemovals.empty()) {
                bi.RemoveBodies(actualRemovals.data(), (int)actualRemovals.size());
                bi.DestroyBodies(actualRemovals.data(), (int)actualRemovals.size());
            }
        }

        for (auto& part : toAdd) {
            part->registeredService = std::static_pointer_cast<PhysicsService>(shared_from_this());
            mPendingAssemblyUpdates.push_back(part);
        }

        for (auto joint : constraintsToAdd) {
            auto p0 = joint->Part0.lock();
            auto p1 = joint->Part1.lock();
            if (!p0 || !p1) continue;

            if (joint->GetClassName() == "Weld" || joint->GetClassName() == "Snap" || 
                joint->GetClassName() == "Glue" || joint->GetClassName() == "AutoJoint") {
                mPartToJoints[p0.get()].push_back(joint);
                mPartToJoints[p1.get()].push_back(joint);
                mPendingAssemblyUpdates.push_back(p0);
                mPendingAssemblyUpdates.push_back(p1);
                PartPair pair = { reinterpret_cast<uint64_t>(p0.get()), reinterpret_cast<uint64_t>(p1.get()) };
                if (pair.first > pair.second) std::swap(pair.first, pair.second);
                {
                    std::unique_lock<std::shared_mutex> lock(mJoinedPairsMutex);
                    mJoinedPairs.insert(pair);
                }
                continue;
            }

            if (p0->physicsBodyID.IsInvalid() || p1->physicsBodyID.IsInvalid()) {
                mPendingConstraints.push_back(joint);
                continue;
            }

            JPH::BodyID ids[] = { p0->physicsBodyID, p1->physicsBodyID };
            JPH::Constraint* c = nullptr;
            {
                JPH::BodyLockMultiWrite multiLock(physicsSystem->GetBodyLockInterface(), ids, 2);
                if (multiLock.GetBody(0) && multiLock.GetBody(1)) {
                    std::shared_lock<std::shared_mutex> mapLock(mMapsMutex);
                    auto a0 = mPartToAssembly[p0.get()];
                    auto a1 = mPartToAssembly[p1.get()];
                    if (!a0 || !a1) continue;
                    CFrame rel0 = a0->relativeTransforms.at(p0.get());
                    CFrame rel1 = a1->relativeTransforms.at(p1.get());

                    if (joint->GetClassName() == "Motor") {
                        JPH::HingeConstraintSettings settings;
                        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                        auto cf0_part = static_cast<Motor*>(joint.get())->props.base.get().C0.get().to_nova();
                        auto cf1_part = static_cast<Motor*>(joint.get())->props.base.get().C1.get().to_nova();
                        CFrame cf0 = rel0 * cf0_part;
                        CFrame cf1 = rel1 * cf1_part;
                        settings.mPoint1 = JPH::RVec3(cf0.position.x, cf0.position.y, cf0.position.z);
                        settings.mPoint2 = JPH::RVec3(cf1.position.x, cf1.position.y, cf1.position.z);
                        settings.mHingeAxis1 = JPH::Vec3(cf0.rotation[0].x, cf0.rotation[0].y, cf0.rotation[0].z);
                        settings.mHingeAxis2 = JPH::Vec3(cf1.rotation[0].x, cf1.rotation[0].y, cf1.rotation[0].z);
                        settings.mNormalAxis1 = JPH::Vec3(cf0.rotation[1].x, cf0.rotation[1].y, cf0.rotation[1].z);
                        settings.mNormalAxis2 = JPH::Vec3(cf1.rotation[1].x, cf1.rotation[1].y, cf1.rotation[1].z);
                        c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                    }
                    else if (joint->GetClassName() == "Hinge") {
                        JPH::HingeConstraintSettings settings;
                        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                        auto cf0_part = static_cast<Hinge*>(joint.get())->props.base.get().C0.get().to_nova();
                        auto cf1_part = static_cast<Hinge*>(joint.get())->props.base.get().C1.get().to_nova();
                        CFrame cf0 = rel0 * cf0_part;
                        CFrame cf1 = rel1 * cf1_part;
                        settings.mPoint1 = JPH::RVec3(cf0.position.x, cf0.position.y, cf0.position.z);
                        settings.mPoint2 = JPH::RVec3(cf1.position.x, cf1.position.y, cf1.position.z);
                        settings.mHingeAxis1 = JPH::Vec3(cf0.rotation[0].x, cf0.rotation[0].y, cf0.rotation[0].z);
                        settings.mHingeAxis2 = JPH::Vec3(cf1.rotation[0].x, cf1.rotation[0].y, cf1.rotation[0].z);
                        settings.mNormalAxis1 = JPH::Vec3(cf0.rotation[1].x, cf0.rotation[1].y, cf0.rotation[1].z);
                        settings.mNormalAxis2 = JPH::Vec3(cf1.rotation[1].x, cf1.rotation[1].y, cf1.rotation[1].z);
                        c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                    }
                    else if (joint->GetClassName() == "VelocityMotor") {
                        JPH::HingeConstraintSettings settings;
                        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                        auto cf0_part = static_cast<VelocityMotor*>(joint.get())->props.base.get().C0.get().to_nova();
                        auto cf1_part = static_cast<VelocityMotor*>(joint.get())->props.base.get().C1.get().to_nova();
                        CFrame cf0 = rel0 * cf0_part;
                        CFrame cf1 = rel1 * cf1_part;
                        settings.mPoint1 = JPH::RVec3(cf0.position.x, cf0.position.y, cf0.position.z);
                        settings.mPoint2 = JPH::RVec3(cf1.position.x, cf1.position.y, cf1.position.z);
                        settings.mHingeAxis1 = JPH::Vec3(cf0.rotation[0].x, cf0.rotation[0].y, cf0.rotation[0].z);
                        settings.mHingeAxis2 = JPH::Vec3(cf1.rotation[0].x, cf1.rotation[0].y, cf1.rotation[0].z);
                        settings.mNormalAxis1 = JPH::Vec3(cf0.rotation[1].x, cf0.rotation[1].y, cf0.rotation[1].z);
                        settings.mNormalAxis2 = JPH::Vec3(cf1.rotation[1].x, cf1.rotation[1].y, cf1.rotation[1].z);
                        c = settings.Create(*multiLock.GetBody(0), *multiLock.GetBody(1));
                        if (c) {
                            auto* h = static_cast<JPH::HingeConstraint*>(c);
                            h->SetMotorState(JPH::EMotorState::Velocity);
                            h->SetTargetAngularVelocity(static_cast<VelocityMotor*>(joint.get())->props.MaxVelocity);
                        }
                    }

                    if (c) {
                        physicsSystem->AddConstraint(c);
                        joint->physicsConstraint = c;
                        joint->registeredService = std::static_pointer_cast<PhysicsService>(shared_from_this());
                        a0->attachedConstraints.insert(c);
                        a1->attachedConstraints.insert(c);
                    }
                }
            }
        }

        for (auto& req : autoJoints) {
            auto p1 = req.part1.lock();
            auto p2 = req.part2.lock();
            if (!p1 || !p2) continue;
            auto activeReq = std::make_shared<InternalJoint>();
            activeReq->part1 = req.part1;
            activeReq->part2 = req.part2;
            mActiveAutoJoints.push_back(activeReq);
            mPartToAutoJoints[p1.get()].push_back(activeReq);
            mPartToAutoJoints[p2.get()].push_back(activeReq);
            mPendingAssemblyUpdates.push_back(p1);
            mPendingAssemblyUpdates.push_back(p2);
            PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
            if (pair.first > pair.second) std::swap(pair.first, pair.second);
            {
                std::unique_lock<std::shared_mutex> lock(mJoinedPairsMutex);
                mJoinedPairs.insert(pair);
            }
        }
    }
}
