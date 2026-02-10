#pragma once
#include <rfl/Flatten.hpp>
#include <rfl/Field.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/BasePart.hpp"
namespace Nova {

    namespace Props {
        struct PartProps {
            rfl::Flatten<BasePartProps> base;

            rfl::Rename<"shape", PartType> shape = PartType::Block;
        };
    }


    class Part : public BasePart {
    public:
        Props::PartProps props;
        // when Part is used as a Base for SpawnLocation or Seat
        Part(std::string name) : BasePart(name) {}

        // standalone Part
        Part() : BasePart("Part") {}

        NOVA_OBJECT(Part, props)
    };

}
