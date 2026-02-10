#include "Engine/Reflection/InstanceFactory.hpp"
#include "Engine/Reflection/LevelLoader.hpp"
#include "Common/MathTypes.hpp"
#include <map>
#include <string>
#include <pugixml.hpp>
#include "Engine/Nova.hpp"

namespace Nova {

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

        auto roblox = doc.child("roblox");

        // Pass 1: Build the tree and the Referent Map
        for (auto item : roblox.children("Item")) {
            ProcessItemPass1(item, dataModel);
        }

        // Pass 2: Resolve References (PrimaryPart, etc.)
        for (auto item : roblox.children("Item")) {
            ProcessItemPass2(item);
        }

        referentMap.clear(); // Clean up memory
    }

    void LevelLoader::ProcessItemPass1(pugi::xml_node node, std::shared_ptr<Instance> parent) {
        const char* className = node.attribute("class").value();

        // Use the className you already extracted at the top of the function
        std::cout << "[DEBUG] Creating: " << className
                  << " | Parent: " << (parent ? "Has Parent" : "Root/Null") << std::endl;
        std::string refId = std::string(node.attribute("referent").value());


        // 1. Check if this is a Service that already exists in the parent
        // (e.g., Workspace, Lighting inside the DataModel)
        std::shared_ptr<Instance> inst = FindService(parent, className);


        if (!inst) {
            // Only create it if it doesn't already exist
            inst = InstanceFactory::Get().Create(className);
            if (!inst) return;
            inst->SetParent(parent);
        }

        if (!refId.empty()) referentMap[refId] = inst;

        // 2. Build the Property Map (The bag of data for reflect-cpp)
        rfl::Object<rfl::Generic> propMap;
        auto propsNode = node.child("Properties");

        for (auto prop : propsNode.children()) {
            std::string type = prop.name();
            std::string name = prop.attribute("name").value();

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

                // CRITICAL: The key here MUST match the variable name in your PartProps struct.
                // If your struct has 'CFrameReflect CFrame;', this MUST be "CFrame".
                // If your struct has 'CFrameReflect cframe;', this MUST be "cframe".
                propMap[name] = rfl::Generic(cf);
            }
            else if (type == "Color3") {
                // stoul handles the large unsigned integer string
                uint32_t packed = std::stoul(prop.text().get());
                rfl::Object<rfl::Generic> col;

                // 2007 Format: Usually GBR or RGB packed into the integer
                // We'll extract as standard RGB bytes
                col["r"] = rfl::Generic((float)((packed >> 16) & 0xFF) / 255.0f);
                col["g"] = rfl::Generic((float)((packed >> 8) & 0xFF) / 255.0f);
                col["b"] = rfl::Generic((float)(packed & 0xFF) / 255.0f);
                propMap[name] = rfl::Generic(col);
            }
        }

        // 3. Apply everything via reflection
        inst->ApplyPropertiesGeneric(rfl::Generic(propMap));

        // 4. Recurse children
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
                    // Since this isn't in 'props', we might need a manual setter or cast
                    // Example: if(auto m = dynamic_cast<Model*>(inst.get())) m->primaryPart = target;
                }
                else if (propName == "CurrentCamera") {
                    // Workspace logic
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
