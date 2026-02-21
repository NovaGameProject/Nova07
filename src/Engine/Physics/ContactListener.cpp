// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "ContactListener.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Objects/BasePart.hpp"
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <iostream>

namespace Nova {

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
        // Stricter alignment (approx 2.5 degrees)
        return (ax > 0.999f && ay < 0.01f && az < 0.01f) ||
               (ay > 0.999f && ax < 0.01f && az < 0.01f) ||
               (az > 0.999f && ax < 0.01f && ay < 0.01f);
    }

    std::shared_ptr<BasePart> ContactListenerImpl::GetPartFromSubShape(const JPH::Body& body, const JPH::SubShapeID& subShapeID) {
        std::shared_lock<std::shared_mutex> mapLock(service->mMapsMutex);
        auto it = service->mBodyToAssembly.find(body.GetID());
        if (it == service->mBodyToAssembly.end()) return nullptr;

        auto assembly = it->second;
        if (assembly->parts.empty()) return nullptr;

        uint32_t index = (uint32_t)body.GetShape()->GetSubShapeUserData(subShapeID);

        if (index < assembly->parts.size()) {
            return assembly->parts[index].lock();
        }
        return nullptr;
    }

    JPH::ValidateResult ContactListenerImpl::OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) {
        auto p1 = GetPartFromSubShape(inBody1, inCollisionResult.mSubShapeID1);
        auto p2 = GetPartFromSubShape(inBody2, inCollisionResult.mSubShapeID2);

        if (p1 && p2) {
             std::shared_lock<std::shared_mutex> lock(service->mJoinedPairsMutex);

             PhysicsService::PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
             if (pair.first > pair.second) std::swap(pair.first, pair.second);

             if (service->mJoinedPairs.contains(pair)) {
                 return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
             }
        }
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void ContactListenerImpl::OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) {
        auto p1 = GetPartFromSubShape(inBody1, inManifold.mSubShapeID1);
        auto p2 = GetPartFromSubShape(inBody2, inManifold.mSubShapeID2);

        if (p1 && p2) {
            {
                std::lock_guard<std::mutex> lock(service->mContactMutex);
                service->mContactBuffer.push_back({ p1, p2 });
            }

            // JOINING LOGIC
            // 1. Check relative velocity - only join if nearly stationary relative to each other
            JPH::Vec3 v1 = inBody1.GetLinearVelocity();
            JPH::Vec3 v2 = inBody2.GetLinearVelocity();
            JPH::Vec3 rv = v1 - v2;
            if (rv.LengthSq() > 0.5f) return; 

            JPH::Vec3 worldNormal = inManifold.mWorldSpaceNormal;
            JPH::RMat44 invM1 = inBody1.GetInverseCenterOfMassTransform();
            JPH::RMat44 invM2 = inBody2.GetInverseCenterOfMassTransform();

            JPH::Vec3 localN1 = invM1.Multiply3x3(worldNormal);
            JPH::Vec3 localN2 = invM2.Multiply3x3(-worldNormal);

            SurfaceType s1 = p1->GetSurfaceType(glm::vec3(localN1.GetX(), localN1.GetY(), localN1.GetZ()));
            SurfaceType s2 = p2->GetSurfaceType(glm::vec3(localN2.GetX(), localN2.GetY(), localN2.GetZ()));

            // 2. Check if surfaces are compatible and aligned
            if (AreSurfacesCompatible(s1, s2) &&
                IsAligned(glm::vec3(localN1.GetX(), localN1.GetY(), localN1.GetZ())) &&
                IsAligned(glm::vec3(localN2.GetX(), localN2.GetY(), localN2.GetZ())))
            {
                // 3. Manifold check - need at least 4 points for a stable face contact
                // AND check penetration depth - don't join if too far or too deep (indicates glitch)
                if (inManifold.mRelativeContactPointsOn1.size() >= 4 && 
                    std::abs(inManifold.mPenetrationDepth) < 0.1f)
                {
                    PhysicsService::PartPair pair = { reinterpret_cast<uint64_t>(p1.get()), reinterpret_cast<uint64_t>(p2.get()) };
                    if (pair.first > pair.second) std::swap(pair.first, pair.second);

                    bool alreadyJoined = false;
                    {
                        std::shared_lock<std::shared_mutex> lock(service->mJoinedPairsMutex);
                        alreadyJoined = service->mJoinedPairs.contains(pair);
                    }

                    if (!alreadyJoined) {
                        std::lock_guard<std::recursive_mutex> lock(service->mQueueMutex);
                        service->mPendingAutoJoints.push_back({ p1, p2, s1, s2 });
                    }
                }
            }
        }
    }

    void JointBreakCollector::AddHit(const JPH::CollideShapeResult &inResult) {
        std::shared_lock<std::shared_mutex> mapLock(service->mMapsMutex);
        auto it = service->mBodyToAssembly.find(inResult.mBodyID2);
        if (it != service->mBodyToAssembly.end()) {
            auto assembly = it->second;
            uint32_t index = 0;
            JPH::BodyLockRead lock(service->GetPhysicsSystem()->GetBodyLockInterface(), inResult.mBodyID2);
            if (lock.Succeeded()) {
                const JPH::Body& body = lock.GetBody();
                index = (uint32_t)body.GetShape()->GetSubShapeUserData(inResult.mSubShapeID2);
            }

            if (index < assembly->parts.size()) {
                if (auto part = assembly->parts[index].lock()) {
                    std::cout << "[JointBreakCollector] Found part: " << part->GetName() << std::endl;
                    mapLock.unlock();
                    service->BreakJoints(part.get());
                }
            }
        }
    }
}
