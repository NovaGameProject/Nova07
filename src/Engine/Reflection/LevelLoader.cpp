// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Reflection/InstanceFactory.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Common/MathTypes.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Objects/Model.hpp"
#include <SDL3/SDL_log.h>
#include <map>
#include <string>
#include <pugixml.hpp>
#include "Engine/Nova.hpp"

namespace Nova {

    // Helper to find all BaseParts in a tree
    void FindAllBaseParts(std::shared_ptr<Instance> inst, std::vector<std::shared_ptr<BasePart>>& out) {
        if (auto bp = std::dynamic_pointer_cast<BasePart>(inst)) {
            out.push_back(bp);
        }
        for (auto& child : inst->GetChildren()) {
            FindAllBaseParts(child, out);
        }
    }

    // This map stores <ReferentID, InstancePointer>
    // Example: <"RBX0", shared_ptr<Part>>
    std::map<std::string, std::shared_ptr<Instance>> referentMap;

    void LevelLoader::PrintInstanceTree(std::shared_ptr<Nova::Instance> instance, int depth) {
        if (!instance) return;

        std::string indent(depth * 2, ' ');
        std::cout << indent << "[" << instance->GetClassName() << "] "
                  << instance->GetName() << std::endl;

        rfl::Generic genericProps = instance->GetPropertiesGeneric();
        auto var = genericProps.variant();

        if (auto* obj = std::get_if<rfl::Object<rfl::Generic>>(&var)) {
            for (const auto& [propName, propValue] : *obj) {
                if (propName == "Name") continue;

                std::cout << indent << "  prop " << propName << ": ";

                // Helper to print values, including nested objects (Vector3, etc)
                auto debugPrint = [](auto& self, const rfl::Generic& g) -> void {
                    auto v = g.variant();
                    if (auto* s = std::get_if<std::string>(&v)) std::cout << "\"" << *s << "\"";
                    else if (auto* d = std::get_if<double>(&v)) std::cout << *d;
                    else if (auto* i = std::get_if<long>(&v)) std::cout << *i;
                    else if (auto* b = std::get_if<bool>(&v)) std::cout << (*b ? "true" : "false");
                    else if (auto* o = std::get_if<rfl::Object<rfl::Generic>>(&v)) {
                        std::cout << "{ ";
                        for (auto it = o->begin(); it != o->end(); ++it) {
                            std::cout << it->first << ": ";
                            self(self, it->second); // Recursion!
                            if (std::next(it) != o->end()) std::cout << ", ";
                        }
                        std::cout << " }";
                    }
                    else std::cout << "unknown";
                };

                debugPrint(debugPrint, propValue);
                std::cout << std::endl;
            }
        }

        for (auto& child : instance->children) {
            PrintInstanceTree(child, depth + 1);
        }
    }

    std::shared_ptr<Nova::Instance> FindService(std::shared_ptr<Nova::Instance> parent, const std::string& className) {
        if (!parent) return nullptr;
        for (auto& child : parent->GetChildren()) {
            if (child->GetClassName() == className) {
                return child;
            }
        }
        return nullptr;
    }

    void LevelLoader::Load(const std::string& path, std::shared_ptr<Instance> dataModel) {
        pugi::xml_document doc;
        if (!doc.load_file(path.c_str())) return;

        std::shared_ptr<PhysicsService> physics = nullptr;
        if (auto dm = std::dynamic_pointer_cast<DataModel>(dataModel)) {
            physics = dm->GetService<PhysicsService>();
            if (physics) physics->SetDeferRegistration(true);
        }

        auto roblox = doc.child("roblox");

        // Pass 1: Build the tree and the Referent Map
        for (auto item : roblox.children("Item")) {
            ProcessItemPass1(item, dataModel);
        }

        // Pass 2: Resolve References (PrimaryPart, etc.)
        for (auto item : roblox.children("Item")) {
            ProcessItemPass2(item);
        }

        // Finalize: Ensure Workspace has a CurrentCamera
        if (auto dm = std::dynamic_pointer_cast<DataModel>(dataModel)) {
            auto workspace = dm->GetService<Workspace>();
            if (!workspace->CurrentCamera) {
                // Look for any camera in the children
                for (auto& child : workspace->GetChildren()) {
                    if (auto cam = std::dynamic_pointer_cast<Camera>(child)) {
                        workspace->CurrentCamera = cam;
                        break;
                    }
                }

                // Still no camera? Create one.
                if (!workspace->CurrentCamera) {
                    auto cam = std::make_shared<Camera>();
                    cam->SetParent(workspace);
                    workspace->CurrentCamera = cam;
                    SDL_Log("No camera found in file, created default Camera.");
                }
            }

            workspace->RefreshCachedParts();
        }

        if (physics) {
            physics->SetDeferRegistration(false);
            
            // Initialize physics interpolation state for all parts
            std::vector<std::shared_ptr<BasePart>> parts;
            FindAllBaseParts(dataModel, parts);
            for (auto& part : parts) part->InitializePhysics();
        }

        referentMap.clear(); // Clean up memory
    }

    void LevelLoader::ProcessItemPass1(pugi::xml_node node, std::shared_ptr<Instance> parent) {
        const char* className = node.attribute("class").value();
        std::string refId = node.attribute("referent").value();

        std::shared_ptr<Instance> inst = nullptr;

        // List of classes that should be treated as singletons/services
        static const std::set<std::string> serviceClasses = {
            "Workspace", "Lighting", "RunService", "Selection", "Debris"
        };

        if (serviceClasses.contains(className)) {
            inst = FindService(parent, className);
        }

        if (!inst) {
            inst = InstanceFactory::Get().Create(className);
            if (!inst) return;
        }

        if (!refId.empty()) referentMap[refId] = inst;

        // 2. Build the Property Map (The bag of data for reflect-cpp)
        rfl::Object<rfl::Generic> propMap;
        auto propsNode = node.child("Properties");

        for (auto prop : propsNode.children()) {
            std::string type = prop.name();
            std::string name = prop.attribute("name").value();

            // Normalize names to match C++ struct (PascalCase)
            if (name == "anchored") name = "Anchored";
            if (name == "canCollide") name = "CanCollide";
            if (name == "CoordinateFrame") name = "CFrame";
            if (name == "size") name = "Size";
            if (name == "archivable") name = "Archivable";
            if (name == "name") name = "Name";

            if (type == "string") propMap[name] = rfl::Generic(std::string(prop.text().get()));
            else if (type == "bool")   propMap[name] = rfl::Generic(prop.text().as_bool());
            else if (type == "float")  propMap[name] = rfl::Generic(prop.text().as_float());
            else if (type == "token" || type == "int") {
                // Enums/Tokens must be int64_t for rfl::Generic to be safe
                propMap[name] = rfl::Generic(static_cast<int64_t>(prop.text().as_int()));
            }


            else if (type == "Vector3") {
                rfl::Object<rfl::Generic> v3;
                v3["x"] = rfl::Generic(prop.child("X").text().as_float());
                v3["y"] = rfl::Generic(prop.child("Y").text().as_float());
                v3["z"] = rfl::Generic(prop.child("Z").text().as_float());
                propMap[name] = rfl::Generic(v3);
            }
            else if (type == "CoordinateFrame") {
                rfl::Object<rfl::Generic> cf;

                // Explicitly cast to float. reflect-cpp is very picky!
                cf["x"] = rfl::Generic(static_cast<float>(prop.child("X").text().as_float()));
                cf["y"] = rfl::Generic(static_cast<float>(prop.child("Y").text().as_float()));
                cf["z"] = rfl::Generic(static_cast<float>(prop.child("Z").text().as_float()));

                cf["r00"] = rfl::Generic(static_cast<float>(prop.child("R00").text().as_float()));
                cf["r01"] = rfl::Generic(static_cast<float>(prop.child("R01").text().as_float()));
                cf["r02"] = rfl::Generic(static_cast<float>(prop.child("R02").text().as_float()));

                cf["r10"] = rfl::Generic(static_cast<float>(prop.child("R10").text().as_float()));
                cf["r11"] = rfl::Generic(static_cast<float>(prop.child("R11").text().as_float()));
                cf["r12"] = rfl::Generic(static_cast<float>(prop.child("R12").text().as_float()));

                cf["r20"] = rfl::Generic(static_cast<float>(prop.child("R20").text().as_float()));
                cf["r21"] = rfl::Generic(static_cast<float>(prop.child("R21").text().as_float()));
                cf["r22"] = rfl::Generic(static_cast<float>(prop.child("R22").text().as_float()));

                propMap[name] = rfl::Generic(cf);
            }
            else if (type == "Color3") {
                rfl::Object<rfl::Generic> col;
                std::string val = prop.text().get();
                if (val.find(',') != std::string::npos) {
                    // Float triplet (0.1, 0.5, 0.8)
                    std::stringstream ss(val);
                    std::string segment;
                    float channels[3] = {0,0,0};
                    for(int i=0; i<3 && std::getline(ss, segment, ','); ++i) {
                        channels[i] = std::stof(segment);
                    }
                    col["r"] = rfl::Generic(channels[0]);
                    col["g"] = rfl::Generic(channels[1]);
                    col["b"] = rfl::Generic(channels[2]);
                } else {
                    // stoul handles the large unsigned integer string
                    uint32_t packed = std::stoul(val);
                    col["r"] = rfl::Generic((float)((packed >> 16) & 0xFF) / 255.0f);
                    col["g"] = rfl::Generic((float)((packed >> 8) & 0xFF) / 255.0f);
                    col["b"] = rfl::Generic((float)(packed & 0xFF) / 255.0f);
                }
                propMap[name] = rfl::Generic(col);
            }
        }

        // 3. Apply everything via reflection
        inst->ApplyPropertiesGeneric(rfl::Generic(propMap));

        // 4. Set parent (triggers OnAncestorChanged and physics registration)
        if (parent && !inst->GetParent()) {
            inst->SetParent(parent);
        }

        // 5. Recurse children
        for (auto child : node.children("Item")) {
            ProcessItemPass1(child, inst);
        }
    }

    void LevelLoader::ProcessItemPass2(pugi::xml_node node) {
        std::string refId = std::string(node.attribute("referent").value());
        auto inst = referentMap[refId];
        if (!inst) return;

        auto propsNode = node.child("Properties");
        for (auto prop : propsNode.children("Ref")) {
            std::string propName = prop.attribute("name").value();
            std::string targetRef = prop.text().get();

            if (targetRef != "null" && referentMap.contains(targetRef)) {
                auto target = referentMap[targetRef];

                // Handle specific non-reflected references
                if (propName == "PrimaryPart") {
                    if (auto mod = std::dynamic_pointer_cast<Model>(inst)) {
                        mod->PrimaryPart = target;
                    }
                }
                else if (propName == "Part0" || propName == "part0") {
                    if (auto joint = std::dynamic_pointer_cast<JointInstance>(inst)) {
                        joint->Part0 = std::dynamic_pointer_cast<BasePart>(target);
                    }
                }
                else if (propName == "Part1" || propName == "part1") {
                    if (auto joint = std::dynamic_pointer_cast<JointInstance>(inst)) {
                        joint->Part1 = std::dynamic_pointer_cast<BasePart>(target);
                    }
                }
                else if (propName == "CurrentCamera") {
                    if (auto ws = std::dynamic_pointer_cast<Workspace>(inst)) {
                        if (auto cam = std::dynamic_pointer_cast<Camera>(target)) {
                            ws->CurrentCamera = cam;
                        }
                    }
                }
            }
        }

        for (auto child : node.children("Item")) {
            ProcessItemPass2(child);
        }
    }

    // Specialized 2007 CoordinateFrame Parser
    CFrame Parse2007CFrame(pugi::xml_node node) {
        CFrame cf;
        cf.position = {
            node.child("X").text().as_float(),
            node.child("Y").text().as_float(),
            node.child("Z").text().as_float()
        };
        // Roblox 2007 uses a 3x3 rotation matrix (R00-R22)
        cf.rotation[0][0] = node.child("R00").text().as_float();
        cf.rotation[0][1] = node.child("R01").text().as_float();
        cf.rotation[0][2] = node.child("R02").text().as_float();
        cf.rotation[1][0] = node.child("R10").text().as_float();
        cf.rotation[1][1] = node.child("R11").text().as_float();
        cf.rotation[1][2] = node.child("R12").text().as_float();
        cf.rotation[2][0] = node.child("R20").text().as_float();
        cf.rotation[2][1] = node.child("R21").text().as_float();
        cf.rotation[2][2] = node.child("R22").text().as_float();
        return cf;
    }

    // Specialized 2007 Vector3 Parser
    Vector3 Parse2007Vector3(pugi::xml_node node) {
        return Vector3(
            node.child("X").text().as_float(),
            node.child("Y").text().as_float(),
            node.child("Z").text().as_float()
        );
    }
}
