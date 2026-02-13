// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Engine/Objects/BasePart.hpp"
#include "Engine/Objects/Part.hpp"
#include "Engine/Services/Workspace.hpp"
#include "Engine/Services/DataModel.hpp"
#include "Engine/Services/ScriptContext.hpp"

namespace Nova {
    void RegisterClasses() {
        // We can use the builder to register signals/methods
        ClassDescriptorBuilder<BasePart>("BasePart", "Instance")
            .Signal("Touched", &BasePart::Touched);
        
        ClassDescriptorBuilder<Part>("Part", "BasePart");
        
        ClassDescriptorBuilder<Workspace>("Workspace", "Instance");
        ClassDescriptorBuilder<DataModel>("DataModel", "Instance");
        ClassDescriptorBuilder<ScriptContext>("ScriptContext", "Instance");

        // ... more classes ...
    }
}
