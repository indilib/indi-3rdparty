// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_DEVICE_DETECTOR_HPP_
#define RICOH_CAMERA_SDK_CAMERA_DEVICE_DETECTOR_HPP_

#include <vector>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraDevice;
enum class DeviceInterface;

/**
 * @brief The class to detect a camera device.
 */
class RCSDK_API CameraDeviceDetector {
public:
    /**
     * @brief Detects a camera device.
     *
     * @param inf The interface for connection
     * @return detected devices
     */
    static std::vector<std::shared_ptr<CameraDevice>> detect(DeviceInterface inf);
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_DEVICE_DETECTOR_HPP_
