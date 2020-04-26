// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_ERROR_HPP_
#define RICOH_CAMERA_SDK_ERROR_HPP_

#include <string>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The type of a error code of a request to a camera device.
 */
enum class ErrorCode {
    /**
     * Connection error
     */
    NetworkError,

    /**
     * Device not found
     */
    DeviceNotFound,

    /**
     * Image not found on camera device
     */
    ImageNotFound,

    /**
     * Invalid setting value
     */
    InvalidSetting,

    /**
     * Invalid parameter
     */
    InvalidArgument,

    /**
     * Not enough space on camera device
     */
    NoSpace,

    /**
     * Rejected by the camera state or request conditions
     */
    Forbidden,

    /**
     * Unknown value not supported
     */
    UnknownValue,

    /**
     * Auto focus error
     */
    AFFailed,

    /**
     * Operation incomplete
     */
    Incomplete,

    /**
     * Unknown error
     */
    UnknownError
};

/**
 * @brief This class represents a error of a request to a camera device.
 */
class RCSDK_API Error {
public:
    /**
     * @brief Constructs with the specified error.
     *
     * @param code   The code of the error
     * @param message The detail message of the error
     */
    Error(ErrorCode code, const std::string& message);

    ~Error() = default;

    /**
     * @brief Returns the code of the error.
     *
     * @return The code of the error
     */
    ErrorCode getCode() const;

    /**
     * @brief Returns the detail message of the error.
     *
     * @return The detail message of the error
     */
    const std::string& getMessage() const;

private:
    ErrorCode code_;
    std::string message_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_ERROR_HPP_
