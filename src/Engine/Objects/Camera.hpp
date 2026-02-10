#include <rfl/Flatten.hpp>
#include <rfl/Rename.hpp>
#include "Engine/Enums/Enums.hpp"
#include "Engine/Objects/Instance.hpp"
#include "Common/MathTypes.hpp"

namespace Nova {
    namespace Props {
        struct CameraProps {
            rfl::Flatten<InstanceProps> base;
            rfl::Rename<"CoordinateFrame", CFrameReflect> CFrame;
            CFrameReflect Focus;
            rfl::Rename<"CameraType", enum CameraType> CameraType = CameraType::Fixed;
            // float FieldOfView = 70.0f;

        };
    }

    class Camera : public Instance {
    public:
        Props::CameraProps props;
        Camera() : Instance("Camera") {}

        glm::mat4 GetViewMatrix() {
            // The View Matrix is the inverse of the Camera's CFrame
            return glm::inverse(props.CFrame.get().to_nova().to_mat4());
        }

        NOVA_OBJECT(Camera, props)
    };
}
