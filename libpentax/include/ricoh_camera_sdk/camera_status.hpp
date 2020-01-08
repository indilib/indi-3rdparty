// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_STATUS_HPP_
#define RICOH_CAMERA_SDK_CAMERA_STATUS_HPP_

#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class Capture;

/**
 * @brief The class for camera status.
 */
class RCSDK_API CameraStatus {
public:
    virtual ~CameraStatus() = default;

    /**
     * @brief Returns the battery level(%) of the camera device.
     *
     * @return The battery level(%)
     */
    virtual uint32_t getBatteryLevel() const = 0;

    /**
     * @brief Returns the current capture of the camera device.
     *
     * When new capture is starting, this method returns new reference.
     *
     * @return The current capture, or empty std::shared_ptr if the camera device has never captured
     */
    virtual std::shared_ptr<const Capture> getCurrentCapture() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_STATUS_HPP_
