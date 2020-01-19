// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAPTURE_HPP_
#define RICOH_CAMERA_SDK_CAPTURE_HPP_

#include <string>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CaptureMethod;

/**
 * @brief The type of a state of a capture.
 */
enum class CaptureState {
    /**
     * Unknown state.
     */
    Unknown,

    /**
     * A capture is in progress.
     */
    Executing,

    /**
     * All processing about a capture is completed.
     */
    Complete
};

/**
 * @brief The interface for a camera capture information.
 */
class RCSDK_API Capture {
public:
    virtual ~Capture() = default;

    /**
     * @brief Returns the ID of the capture.
     *
     * @return The ID
     */
    virtual const std::string& getId() const = 0;

    /**
     * @brief Returns the capture method of the capture.
     *
     * @return The capture method
     */
    virtual const CaptureMethod& getMethod() const = 0;

    /**
     * @brief Returns the state of the capture.
     *
     * @return The state
     */
    virtual CaptureState getState() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAPTURE_HPP_
