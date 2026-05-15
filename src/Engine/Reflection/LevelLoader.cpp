// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "Engine/Objects/InstanceFactory.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Engine/Reflection/ClassDescriptor.hpp"
#include "Common/MathTypes.hpp"
#include "Engine/Services/PhysicsService.hpp"
#include "Engine/Objects/Model.hpp"
#include <SDL3/SDL_log.h>
#include <map>
#include <set>
#include <string>
#include <pugixml.hpp>
#include "Engine/Nova.hpp"

namespace Nova {

    void FindAllBaseParts(std::shared_ptr<Instance> inst, std::vector<std::shared_ptr<BasePart>>& out) {
        if (auto bp = std::dynamic_pointer_cast<BasePart>(inst)) {
            out.push_back(bp);
        }
        for (auto& child : inst->GetChildren()) {
            FindAllBaseParts(child, out);
        }
    }

    std::map<std::string, std::shared_ptr<Instance>> referentMap;

    void LevelLoader::PrintInstanceTree(std::shared_ptr<Nova::Instance> instance, int depth) {
        if (!instance) return;

        std::string indent(depth * 2, ' ');
        std::cout << indent << "[" << instance->GetClassName() << "] "
                  << instance->GetName() << std::endl;

        // Print properties via ClassDescriptor
        auto* desc = ClassDescriptor::Get(instance->GetClassName());
        if (desc) {
            for (auto& [name, accessor] : desc->properties) {
                if (name == "Name") continue;
                PropertyValue val = accessor->get(instance.get());
                std::cout << indent << "  prop " << name << ": ";
                switch (val.kind) {
                    case PropertyValue::Kind::Nil: std::cout << "nil"; break;
                    case PropertyValue::Kind::Bool: std::cout << (val.toBool() ? "true" : "false"); break;
                    case PropertyValue::Kind::Int: std::cout << val.toInt(); break;
                    case PropertyValue::Kind::Float: std::cout << val.toFloat(); break;
                    case PropertyValue::Kind::String: std::cout << "\"" << val.toString() << "\""; break;
                    case PropertyValue::Kind::Vector3: {
                        auto& v = val.toVector3();
                        std::cout << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
                        break;
                    }
                    case PropertyValue::Kind::CFrame: {
                        auto& cf = val.toCFrame();
                        std::cout << "CFrame(" << cf.position.x << ", " << cf.position.y << ", " << cf.position.z << ")";
                        break;
                    }
                    case PropertyValue::Kind::Color3: {
                        Color3 c = val.toColor3();
                        std::cout << "Color3(" << c.r << ", " << c.g << ", " << c.b << ")";
                        break;
                    }
                }
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

        for (auto item : roblox.children("Item")) {
            ProcessItemPass1(item, dataModel);
        }

        for (auto item : roblox.children("Item")) {
            ProcessItemPass2(item);
        }

        if (auto dm = std::dynamic_pointer_cast<DataModel>(dataModel)) {
            auto workspace = dm->GetService<Workspace>();
            if (!workspace->CurrentCamera) {
                for (auto& child : workspace->GetChildren()) {
                    if (auto cam = std::dynamic_pointer_cast<Camera>(child)) {
                        workspace->CurrentCamera = cam;
                        break;
                    }
                }

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

            std::vector<std::shared_ptr<BasePart>> parts;
            FindAllBaseParts(dataModel, parts);
            for (auto& part : parts) part->InitializePhysics();
        }

        referentMap.clear();
    }

    void LevelLoader::ProcessItemPass1(pugi::xml_node node, std::shared_ptr<Instance> parent) {
        const char* className = node.attribute("class").value();
        std::string refId = node.attribute("referent").value();

        std::shared_ptr<Instance> inst = nullptr;

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

        // Parse properties from XML into PropertyValue map
        std::map<std::string, PropertyValue> propMap;
        auto propsNode = node.child("Properties");

        for (auto prop : propsNode.children()) {
            std::string type = prop.name();
            std::string name = prop.attribute("name").value();

            // Normalize names to match Roblox API (PascalCase)
            if (name == "anchored") name = "Anchored";
            if (name == "canCollide") name = "CanCollide";
            if (name == "CoordinateFrame") name = "CFrame";
            if (name == "size") name = "Size";
            if (name == "archivable") name = "Archivable";
            if (name == "name") name = "Name";
            if (name == "shape") name = "shape"; // PartType uses lowercase in some files

            if (type == "string") {
                propMap[name] = PropertyValue(std::string(prop.text().get()));
            }
            else if (type == "bool") {
                propMap[name] = PropertyValue(prop.text().as_bool());
            }
            else if (type == "float") {
                propMap[name] = PropertyValue(prop.text().as_float());
            }
            else if (type == "token" || type == "int") {
                propMap[name] = PropertyValue(static_cast<int64_t>(prop.text().as_int()));
            }
            else if (type == "Vector3") {
                propMap[name] = PropertyValue(Vector3(
                    prop.child("X").text().as_float(),
                    prop.child("Y").text().as_float(),
                    prop.child("Z").text().as_float()
                ));
            }
            else if (type == "CoordinateFrame") {
                CFrame cf;
                cf.position = {
                    prop.child("X").text().as_float(),
                    prop.child("Y").text().as_float(),
                    prop.child("Z").text().as_float()
                };
                // XML stores rows (R00,R01,R02 is row 0), GLM stores columns
                // So we need to transpose: column 0 = {R00, R10, R20}
                cf.rotation[0] = glm::vec3(
                    prop.child("R00").text().as_float(),
                    prop.child("R10").text().as_float(),
                    prop.child("R20").text().as_float()
                );
                cf.rotation[1] = glm::vec3(
                    prop.child("R01").text().as_float(),
                    prop.child("R11").text().as_float(),
                    prop.child("R21").text().as_float()
                );
                cf.rotation[2] = glm::vec3(
                    prop.child("R02").text().as_float(),
                    prop.child("R12").text().as_float(),
                    prop.child("R22").text().as_float()
                );
                propMap[name] = PropertyValue(cf);
            }
            else if (type == "Color3") {
                std::string val = prop.text().get();
                Color3 col;
                if (val.find(',') != std::string::npos) {
                    std::stringstream ss(val);
                    std::string segment;
                    float channels[3] = {0,0,0};
                    for (int i = 0; i < 3 && std::getline(ss, segment, ','); ++i) {
                        channels[i] = std::stof(segment);
                    }
                    col = Color3(channels[0], channels[1], channels[2]);
                } else {
                    uint32_t packed = std::stoul(val);
                    col = Color3(
                        (float)((packed >> 16) & 0xFF) / 255.0f,
                        (float)((packed >> 8) & 0xFF) / 255.0f,
                        (float)(packed & 0xFF) / 255.0f
                    );
                }
                propMap[name] = PropertyValue::FromColor3(col);
            }
        }

        // Apply properties via ClassDescriptor
        auto* desc = ClassDescriptor::Get(inst->GetClassName());
        for (const auto& [name, value] : propMap) {
            auto* current = desc;
            while (current) {
                if (auto it = current->properties.find(name); it != current->properties.end()) {
                    it->second->set(inst.get(), value);
                    break;
                }
                current = current->baseClass;
            }
        }

        // Set parent
        if (parent && !inst->GetParent()) {
            inst->SetParent(parent);
        }

        // Recurse children
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
}
