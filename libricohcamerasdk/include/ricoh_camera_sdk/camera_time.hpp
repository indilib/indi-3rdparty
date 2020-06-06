// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_TIME_HPP_
#define RICOH_CAMERA_SDK_CAMERA_TIME_HPP_

#include <ricoh_camera_sdk/camera_device_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief This class for setting of the date and time of camera.
 */
class RCSDK_API CameraTime : public CameraDeviceSetting {
public:
    /**
     * @brief Constructs an object not having a value.
     */
    CameraTime();

    /**
     * @brief Constructs an object having given a value.
     *
     * @param dateTime date and time
     */
    CameraTime(time_t dateTime);

    ~CameraTime() = default;

private:
    static const std::string Name;
};

/**
 * @brief This class for setting value of the date and time of camera.
 */
class RCSDK_API CameraTimeValue : public CameraDeviceSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    CameraTimeValue(time_t dateTime);

    ~CameraTimeValue() = default;

    bool operator==(const CameraDeviceSettingValue& obj) const override;
    bool operator!=(const CameraDeviceSettingValue& obj) const override;

    /**
     * @brief Returns the date and time value.
     *
     * @return date and time value
     */
    time_t get() const;

    std::string toString() const override;

private:
    time_t dateTime_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_TIME_HPP_
