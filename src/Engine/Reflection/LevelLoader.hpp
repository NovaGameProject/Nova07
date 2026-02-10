#pragma once
#include <string>
#include <memory>
#include <map>
#include <pugixml.hpp>
#include "Engine/Nova.hpp"

namespace Nova {

    class LevelLoader {
    public:
        /**
         * @brief Loads a .rbxl file and populates the provided root instance.
         * @param path The filesystem path to the .rbxl file.
         * @param root The root instance (usually the DataModel).
         */
        static void Load(const std::string& path, std::shared_ptr<Instance> root);

        static void PrintInstanceTree(std::shared_ptr<Nova::Instance> instance, int depth = 0);

    private:
        // Pass 1: Hierarchy creation and Referent registration
        static void ProcessItemPass1(pugi::xml_node node, std::shared_ptr<Instance> parent);

        // Pass 2: Reference resolution (Connecting Refs like PrimaryPart)
        static void ProcessItemPass2(pugi::xml_node node);

        // Helper to map XML referent IDs to created C++ Instances
        static inline std::map<std::string, std::shared_ptr<Instance>> referentMap;
    };

}
