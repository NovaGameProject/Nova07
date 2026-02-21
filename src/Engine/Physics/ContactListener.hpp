// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <memory>

namespace Nova {
    class PhysicsService;
    class BasePart;

    class ContactListenerImpl : public JPH::ContactListener {
    public:
        PhysicsService* service;
        ContactListenerImpl(PhysicsService* service) : service(service) {}

        std::shared_ptr<BasePart> GetPartFromSubShape(const JPH::Body& body, const JPH::SubShapeID& subShapeID);

        JPH::ValidateResult OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override;
        void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override;
    };

    class JointBreakCollector : public JPH::CollideShapeCollector {
    public:
        PhysicsService* service;
        JointBreakCollector(PhysicsService* s) : service(s) {}

        void AddHit(const JPH::CollideShapeResult &inResult) override;
    };
}
