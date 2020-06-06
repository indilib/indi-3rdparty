// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_HYPER_OPERATION_ENABLE_HPP_
#define RICOH_CAMERA_SDK_HYPER_OPERATION_ENABLE_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of hyper operation enable.
 */
class RCSDK_API HyperOperationEnable : public CaptureSetting {
public:
    static const HyperOperationEnable* True;
    static const HyperOperationEnable* False;

    /**
     * @brief Constructs an object not having a value.
     */
    HyperOperationEnable();

    ~HyperOperationEnable() = default;

private:
    HyperOperationEnable(bool value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of hyper operation enable.
 */
class RCSDK_API HyperOperationEnableValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    HyperOperationEnableValue(bool value);

    ~HyperOperationEnableValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    /**
     * @brief Returns the setting value as bool.
     *
     * @return value whether hyper operation is enabled.
     */
    bool get() const;

    std::string toString() const override;

private:
    bool value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_HYPER_OPERATION_ENABLE_HPP_

