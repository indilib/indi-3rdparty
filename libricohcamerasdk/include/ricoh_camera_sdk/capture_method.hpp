// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAPTURE_METHOD_HPP_
#define RICOH_CAMERA_SDK_CAPTURE_METHOD_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief This class for setting of capture method. Only use in Capture::getMethod()
 */
class RCSDK_API CaptureMethod : public CaptureSetting {
public:
    /**
     * @brief constant for still image
     */
    static const CaptureMethod* StillImage;

    /**
     * @brief constant for movie
     */
    static const CaptureMethod* Movie;

    /**
     * @brief Constructs an object not having a value.
     */
    CaptureMethod();

    ~CaptureMethod() = default;

private:
    CaptureMethod(const std::string& method);

    static const std::string Name;
};

/**
 * @brief This class for setting value of the capture method.
 */
class RCSDK_API CaptureMethodValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    CaptureMethodValue(const std::string& value);

    ~CaptureMethodValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAPTURE_METHOD_HPP_
