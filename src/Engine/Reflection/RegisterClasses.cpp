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
        // We can use the builder to register signals/methods
        ClassDescriptorBuilder<BasePart>("BasePart", "Instance")
            .Signal("Touched", &BasePart::Touched);

        ClassDescriptorBuilder<Part>("Part", "BasePart");

        ClassDescriptorBuilder<Workspace>("Workspace", "Instance");
        ClassDescriptorBuilder<DataModel>("DataModel", "Instance");
        ClassDescriptorBuilder<ScriptContext>("ScriptContext", "Instance");
        ClassDescriptorBuilder<Model>("Model", "Instance");

        ClassDescriptorBuilder<JointInstance>("JointInstance", "Instance");
        ClassDescriptorBuilder<AutoJoint>("AutoJoint", "JointInstance");
        ClassDescriptorBuilder<Weld>("Weld", "JointInstance");
        ClassDescriptorBuilder<Snap>("Snap", "JointInstance");
        ClassDescriptorBuilder<Glue>("Glue", "JointInstance");
        ClassDescriptorBuilder<Motor>("Motor", "JointInstance");
        ClassDescriptorBuilder<Explosion>("Explosion", "Instance")
            .Signal("Hit", &Explosion::Hit);
        ClassDescriptorBuilder<Script>("Script", "Instance");
        ClassDescriptorBuilder<LocalScript>("LocalScript", "Script");
        ClassDescriptorBuilder<Hinge>("Hinge", "JointInstance");
        ClassDescriptorBuilder<VelocityMotor>("VelocityMotor", "JointInstance");

        // ... more classes ...
    }
}
