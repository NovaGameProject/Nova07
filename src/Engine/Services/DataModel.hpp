#pragma once
#include "Engine/Objects/Instance.hpp"

namespace Nova {
    class DataModel : public Instance {
    public:
        DataModel() : Instance("Game") {}
        NOVA_OBJECT_NO_PROPS(DataModel)

        template<typename T>
        std::shared_ptr<T> GetService() {
            for (auto& child : children) {
                if (auto service = std::dynamic_pointer_cast<T>(child)) return service;
            }
            auto s = std::make_shared<T>();
            s->SetParent(shared_from_this());
            return s;
        }
    };
}
