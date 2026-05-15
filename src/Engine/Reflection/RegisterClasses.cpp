// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Nova.hpp"
#include "Engine/Objects/Explosion.hpp"
#include "Engine/Objects/Script.hpp"
#include "Engine/Objects/LocalScript.hpp"

namespace Nova {
    void RegisterClasses() {
        // BasePart
        ClassDescriptorBuilder<BasePart>("BasePart", "Instance")
            .Property("CFrame", &BasePart::cframe)
            .Property("Size", &BasePart::size)
            .Property("Anchored", &BasePart::anchored)
            .Property("CanCollide", &BasePart::canCollide)
            .Property("Transparency", &BasePart::transparency)
            .Property("BrickColor", &BasePart::brickColor)
            .Property("TopSurface", &BasePart::topSurface)
            .Property("BottomSurface", &BasePart::bottomSurface)
            .Property("LeftSurface", &BasePart::leftSurface)
            .Property("RightSurface", &BasePart::rightSurface)
            .Property("FrontSurface", &BasePart::frontSurface)
            .Property("BackSurface", &BasePart::backSurface)
            .Signal("Touched", &BasePart::Touched)
            .Method("BreakJoints", &BasePart::BreakJoints)
            .Method("GetVelocity", &BasePart::GetVelocity)
            .Method("SetVelocity", &BasePart::SetVelocity);

        // Part
        ClassDescriptorBuilder<Part>("Part", "BasePart")
            .Property("shape", &Part::shape);

        // Seat & SpawnLocation (no extra properties)
        ClassDescriptorBuilder<Seat>("Seat", "Part");
        ClassDescriptorBuilder<SpawnLocation>("SpawnLocation", "Part");

        // Workspace
        ClassDescriptorBuilder<Workspace>("Workspace", "Instance")
            .Property("FallenPartsDestroyHeight", &Workspace::FallenPartsDestroyHeight);

        // DataModel
        ClassDescriptorBuilder<DataModel>("DataModel", "Instance");

        // ScriptContext
        ClassDescriptorBuilder<ScriptContext>("ScriptContext", "Instance");

        // Model
        ClassDescriptorBuilder<Model>("Model", "Instance");

        // Camera
        ClassDescriptorBuilder<Camera>("Camera", "Instance")
            .Property("CFrame", &Camera::cframe)
            .Property("Focus", &Camera::focus)
            .Property("CameraType", &Camera::cameraType)
            .Property("FieldOfView", &Camera::fieldOfView);

        // Sky
        ClassDescriptorBuilder<Sky>("Sky", "Instance")
            .Property("SkyboxBk", &Sky::SkyboxBk)
            .Property("SkyboxDn", &Sky::SkyboxDn)
            .Property("SkyboxFt", &Sky::SkyboxFt)
            .Property("SkyboxLf", &Sky::SkyboxLf)
            .Property("SkyboxRt", &Sky::SkyboxRt)
            .Property("SkyboxUp", &Sky::SkyboxUp)
            .Property("StarCount", &Sky::StarCount);

        // SpecialMesh
        ClassDescriptorBuilder<SpecialMesh>("SpecialMesh", "Instance")
            .Property("MeshId", &SpecialMesh::MeshId)
            .Property("TextureId", &SpecialMesh::TextureId)
            .Property("Scale", &SpecialMesh::Scale);

        // Lighting
        ClassDescriptorBuilder<Lighting>("Lighting", "Instance")
            .Property("TopAmbientV9", &Lighting::TopAmbientV9)
            .Property("BottomAmbientV9", &Lighting::BottomAmbientV9)
            .Property("SpotLightV9", &Lighting::SpotLightV9)
            .Property("ClearColor", &Lighting::ClearColor)
            .Property("GeographicLatitude", &Lighting::GeographicLatitude)
            .Property("TimeOfDay", &Lighting::TimeOfDay);

        // Joint types
        ClassDescriptorBuilder<JointInstance>("JointInstance", "Instance")
            .Property("C0", &JointInstance::c0)
            .Property("C1", &JointInstance::c1);

        ClassDescriptorBuilder<AutoJoint>("AutoJoint", "JointInstance");

        ClassDescriptorBuilder<Weld>("Weld", "JointInstance");
        ClassDescriptorBuilder<Snap>("Snap", "JointInstance");
        ClassDescriptorBuilder<Glue>("Glue", "JointInstance");

        ClassDescriptorBuilder<Motor>("Motor", "JointInstance")
            .Property("MaxVelocity", &Motor::MaxVelocity)
            .Property("DesiredAngle", &Motor::DesiredAngle);

        ClassDescriptorBuilder<Hinge>("Hinge", "JointInstance")
            .Property("LowerAngle", &Hinge::LowerAngle)
            .Property("UpperAngle", &Hinge::UpperAngle)
            .Property("LimitsEnabled", &Hinge::LimitsEnabled)
            .Method("GetCurrentAngle", &Hinge::GetCurrentAngle);

        ClassDescriptorBuilder<VelocityMotor>("VelocityMotor", "JointInstance")
            .Property("MaxVelocity", &VelocityMotor::MaxVelocity)
            .Property("DesiredAngle", &VelocityMotor::DesiredAngle)
            .Method("GetCurrentAngle", &VelocityMotor::GetCurrentAngle)
            .Method("SetTargetVelocity", &VelocityMotor::SetTargetVelocity);

        // Script
        ClassDescriptorBuilder<Script>("Script", "Instance")
            .Property("Source", &Script::Source)
            .Property("Disabled", &Script::Disabled);

        ClassDescriptorBuilder<LocalScript>("LocalScript", "Script");

        // Explosion
        ClassDescriptorBuilder<Explosion>("Explosion", "Instance")
            .Property("Position", &Explosion::position)
            .Property("BlastRadius", &Explosion::BlastRadius)
            .Property("BlastPressure", &Explosion::BlastPressure)
            .Signal("Hit", &Explosion::Hit);

        // Resolve inheritance pointers after all classes are registered
        ClassDescriptor::ResolveInheritance();
    }
}
