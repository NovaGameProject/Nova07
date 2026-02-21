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
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <iostream>

namespace Nova {

    void PhysicsService::UpdateAssemblies() {
        std::vector<std::weak_ptr<BasePart>> updates_weak;
        {
            std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
            updates_weak.swap(mPendingAssemblyUpdates);
        }

        if (updates_weak.empty()) return;

        JPH::BodyInterface& bi = physicsSystem->GetBodyInterface();
        std::unordered_set<BasePart*> visited;

        for (auto& wp : updates_weak) {
            auto startPart = wp.lock();
            if (!startPart || visited.contains(startPart.get())) continue;

            // New component discovery
            std::vector<std::shared_ptr<BasePart>> component;
            std::vector<std::shared_ptr<BasePart>> stack = { startPart };
            bool hasAnchored = false;
            std::shared_ptr<BasePart> bestRoot = nullptr;
            float maxVolume = -1.0f;

            while (!stack.empty()) {
                std::shared_ptr<BasePart> p = stack.back();
                stack.pop_back();
                if (!visited.insert(p.get()).second) continue;
                component.push_back(p);

                if (p->basePartProps && p->basePartProps->Anchored) hasAnchored = true;
                
                glm::vec3 sz = p->GetSize();
                float volume = sz.x * sz.y * sz.z;
                if (volume > maxVolume) {
                    maxVolume = volume;
                    bestRoot = p;
                }

                // Traverse rigid joints
                auto it = mPartToJoints.find(p.get());
                if (it != mPartToJoints.end()) {
                    for (auto& weakJoint : it->second) {
                        if (auto joint = weakJoint.lock()) {
                            if (joint->GetClassName() == "Weld" || joint->GetClassName() == "Snap" || 
                                joint->GetClassName() == "Glue" || joint->GetClassName() == "AutoJoint") {
                                auto p0 = joint->Part0.lock();
                                auto p1 = joint->Part1.lock();
                                std::shared_ptr<BasePart> other = (p0 == p) ? p1 : p0;
                                if (other && !visited.contains(other.get())) stack.push_back(other);
                            }
                        }
                    }
                }

                auto it2 = mPartToAutoJoints.find(p.get());
                if (it2 != mPartToAutoJoints.end()) {
                    for (auto& req : it2->second) {
                        auto p0 = req->part1.lock();
                        auto p1 = req->part2.lock();
                        std::shared_ptr<BasePart> other = (p0 == p) ? p1 : p0;
                        if (other && !visited.contains(other.get())) stack.push_back(other);
                    }
                }
            }

            auto assembly = std::make_shared<Assembly>();
            assembly->rootPart = bestRoot.get();
            assembly->isStatic = hasAnchored;

            CFrame rootCF = bestRoot->basePartProps->CFrame.get().to_nova();
            glm::mat4 invRoot = glm::inverse(rootCF.to_mat4());

            JPH::StaticCompoundShapeSettings compoundSettings;
            uint32_t index = 0;
            for (auto& p : component) {
                assembly->parts.push_back(p);
                CFrame partCF = p->basePartProps->CFrame.get().to_nova();
                CFrame relCF = CFrame::from_mat4(invRoot) * partCF;
                assembly->relativeTransforms[p.get()] = relCF;

                glm::vec3 size = p->GetSize();
                JPH::BoxShapeSettings boxSettings(JPH::Vec3(std::max(0.05f, size.x * 0.5f), std::max(0.05f, size.y * 0.5f), std::max(0.05f, size.z * 0.5f)));
                boxSettings.mDensity = 1.0f;
                
                glm::quat q = glm::normalize(glm::quat_cast(relCF.rotation));
                if (glm::any(glm::isnan(q))) q = glm::quat(1, 0, 0, 0);

                compoundSettings.AddShape(
                    JPH::Vec3(relCF.position.x, relCF.position.y, relCF.position.z),
                    JPH::Quat(q.x, q.y, q.z, q.w),
                    boxSettings.Create().Get(),
                    index++
                );
            }

            auto shapeResult = compoundSettings.Create();
            if (!shapeResult.IsValid()) continue;

            std::unordered_set<JointInstance*> jointsToRebuild;
            JPH::Vec3 oldLinearVel = JPH::Vec3::sZero();
            JPH::Vec3 oldAngularVel = JPH::Vec3::sZero();
            bool velInherited = false;

            {
                std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
                std::unordered_set<JPH::BodyID, BodyIDHasher> uniqueOldBodies;
                for (auto& p : component) {
                    if (!p->physicsBodyID.IsInvalid()) uniqueOldBodies.insert(p->physicsBodyID);
                }

                std::unordered_set<JPH::Constraint*> constraintsToRemove;
                for (auto id : uniqueOldBodies) {
                    auto itBody = mBodyToAssembly.find(id);
                    if (itBody != mBodyToAssembly.end()) {
                        if (!velInherited && bi.GetMotionType(id) == JPH::EMotionType::Dynamic) {
                            oldLinearVel = bi.GetLinearVelocity(id);
                            oldAngularVel = bi.GetAngularVelocity(id);
                            velInherited = true;
                        }

                        for (auto* constraint : itBody->second->attachedConstraints) {
                            constraintsToRemove.insert(constraint);
                        }
                    }
                }

                for (auto* constraint : constraintsToRemove) {
                    for (auto& [id, ass] : mBodyToAssembly) {
                        if (ass->attachedConstraints.contains(constraint)) {
                            for (auto& wp : ass->parts) {
                                if (auto p = wp.lock()) {
                                    auto itJ = mPartToJoints.find(p.get());
                                    if (itJ != mPartToJoints.end()) {
                                        for (auto& weakJoint : itJ->second) {
                                            if (auto joint = weakJoint.lock()) {
                                                if (joint->physicsConstraint == constraint) {
                                                    jointsToRebuild.insert(joint.get());
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

                for (auto id : uniqueOldBodies) {
                    if (mAllActiveBodies.contains(id)) {
                        bi.RemoveBody(id);
                        bi.DestroyBody(id);
                        mAllActiveBodies.erase(id);
                    }
                    mBodyToAssembly.erase(id);
                }

                for (auto& p : component) {
                    p->physicsBodyID = JPH::BodyID();
                    mPartToAssembly.erase(p.get());
                }
            }

            JPH::EMotionType motionType = hasAnchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
            JPH::ObjectLayer layer = hasAnchored ? Layers::NON_MOVING : Layers::MOVING;

            glm::quat rootQ = glm::normalize(glm::quat_cast(rootCF.rotation));
            if (glm::any(glm::isnan(rootQ))) rootQ = glm::quat(1, 0, 0, 0);

            JPH::BodyCreationSettings bodySettings(
                shapeResult.Get(),
                JPH::RVec3(rootCF.position.x, rootCF.position.y, rootCF.position.z),
                JPH::Quat(rootQ.x, rootQ.y, rootQ.z, rootQ.w),
                motionType,
                layer
            );
            bodySettings.mAllowSleeping = true;
            bodySettings.mFriction = 0.5f;
            bodySettings.mRestitution = 0.1f;

            JPH::Body* body = bi.CreateBody(bodySettings);
            if (body) {
                bi.AddBody(body->GetID(), JPH::EActivation::Activate);
                assembly->bodyID = body->GetID();

                if (velInherited && motionType == JPH::EMotionType::Dynamic) {
                    bi.SetLinearVelocity(body->GetID(), oldLinearVel);
                    bi.SetAngularVelocity(body->GetID(), oldAngularVel);
                }

                {
                    std::unique_lock<std::shared_mutex> mapLock(mMapsMutex);
                    mBodyToAssembly[body->GetID()] = assembly;
                    mAllActiveBodies.insert(body->GetID());
                    for (auto& p : component) {
                        p->physicsBodyID = body->GetID();
                        mPartToAssembly[p.get()] = assembly;
                    }
                }

                for (auto* joint : jointsToRebuild) {
                    std::lock_guard<std::recursive_mutex> lock(mQueueMutex);
                    mPendingConstraints.push_back(std::static_pointer_cast<JointInstance>(joint->shared_from_this()));
                }
            }
        }
    }
}
