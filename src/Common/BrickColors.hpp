#pragma once
#include <map>
#include <glm/glm.hpp>

namespace Nova {
    struct BrickColorUtils {
        static glm::vec3 ToColor3(int id) {
            static const std::map<int, glm::vec3> colorMap = {
                {1,    {242/255.0f, 243/255.0f, 243/255.0f}}, // White
                {2,    {161/255.0f, 165/255.0f, 162/255.0f}}, // Grey
                {3,    {249/255.0f, 233/255.0f, 153/255.0f}}, // Light yellow
                {5,    {196/255.0f, 40/255.0f, 28/255.0f}},   // Red (Bright red in 2007)
                {9,    {245/255.0f, 205/255.0f, 48/255.0f}},  // Bright yellow
                {11,   {194/255.0f, 218/255.0f, 184/255.0f}}, // Pastel Blue (Used for Mint/Light green sometimes)
                {12,   {203/255.0f, 132/255.0f, 66/255.0f}},  // Light orange brown
                {18,   {204/255.0f, 142/255.0f, 105/255.0f}}, // Nougat
                {21,   {196/255.0f, 40/255.0f, 28/255.0f}},   // Bright red
                {23,   {13/255.0f, 105/255.0f, 172/255.0f}},  // Bright blue
                {24,   {245/255.0f, 205/255.0f, 48/255.0f}},  // Bright yellow
                {25,   {98/255.0f, 71/255.0f, 50/255.0f}},    // Earth orange
                {26,   {27/255.0f, 42/255.0f, 53/255.0f}},    // Black
                {27,   {109/255.0f, 110/255.0f, 108/255.0f}}, // Dark grey
                {28,   {40/255.0f, 127/255.0f, 71/255.0f}},   // Dark green
                {37,   {75/255.0f, 151/255.0f, 75/255.0f}},   // Bright green
                {38,   {160/255.0f, 95/255.0f, 53/255.0f}},   // Dark orange
                {101,  {110/255.0f, 153/255.0f, 202/255.0f}}, // Medium blue
                {102,  {9/255.0f, 137/255.0f, 207/255.0f}},   // Electric blue
                {103,  {199/255.0f, 193/255.0f, 183/255.0f}}, // Light grey
                {104,  {107/255.0f, 50/255.0f, 124/255.0f}},  // Bright violet
                {105,  {226/255.0f, 155/255.0f, 64/255.0f}},  // Br. yellowish orange
                {107,  {218/255.0f, 133/255.0f, 65/255.0f}},  // Bright orange
                {108,  {0/255.0f, 143/255.0f, 156/255.0f}},   // Bright bluish green
                {110,  {104/255.0f, 92/255.0f, 67/255.0f}},   // Earth yellow
                {111,  {67/255.0f, 84/255.0f, 147/255.0f}},   // Bright bluish violet
                {119,  {183/255.0f, 215/255.0f, 213/255.0f}}, // Light bluish green
                {124,  {211/255.0f, 111/255.0f, 76/255.0f}},  // Br. reddish orange
                {135,  {116/255.0f, 134/255.0f, 157/255.0f}}, // Sand blue
                {141,  {149/255.0f, 138/255.0f, 115/255.0f}}, // Sand yellow
                {151,  {120/255.0f, 144/255.0f, 130/255.0f}}, // Sand green
                {154,  {120/255.0f, 144/255.0f, 130/255.0f}}, // Sand green
                {191,  {215/255.0f, 169/255.0f, 75/255.0f}},  // Curry
                {192,  {105/255.0f, 64/255.0f, 40/255.0f}},   // Reddish brown
                {194,  {163/255.0f, 162/255.0f, 165/255.0f}}, // Medium stone grey
                {199,  {99/255.0f, 95/255.0f, 98/255.0f}},    // Dark stone grey
                {208,  {229/255.0f, 228/255.0f, 223/255.0f}}, // Light stone grey
                {217,  {150/255.0f, 112/255.0f, 159/255.0f}}, // Reddish lilac
                {226,  {253/255.0f, 234/255.0f, 141/255.0f}}  // Cool yellow
            };

            auto it = colorMap.find(id);
            if (it != colorMap.end()) {
                return it->second;
            }
            return {163/255.0f, 162/255.0f, 165/255.0f}; // Default to Medium Stone Grey
        }
    };
}
