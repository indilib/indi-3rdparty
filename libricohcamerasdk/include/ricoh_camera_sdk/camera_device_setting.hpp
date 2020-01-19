// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_DEVICE_SETTING_HPP_
#define RICOH_CAMERA_SDK_CAMERA_DEVICE_SETTING_HPP_

#include <string>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraDeviceSettingValue;

/**
 * @brief This class is base class for camera device settings.
 * Sub classes of this class are used to handle camera device settings.
 */
class RCSDK_API CameraDeviceSetting {
public:
    virtual ~CameraDeviceSetting() = default;

    virtual bool operator==(const CameraDeviceSetting& obj) const;
    virtual bool operator!=(const CameraDeviceSetting& obj) const;

    /**
     * @brief Returns the name of the setting
     *
     * @return The name of the setting
     */
    virtual const std::string& getName() const;

    /**
     * @brief Returns the value of a setting
     *
     * @return The value of a setting
     */
    virtual const CameraDeviceSettingValue& getValue() const;

    /**
     * @brief Sets the value of a setting
     *
     * @param value The value of a setting
     */
    virtual void setValue(std::unique_ptr<CameraDeviceSettingValue>&& value);

    virtual std::string toString() const;

protected:
    CameraDeviceSetting(const std::string& name);
    CameraDeviceSetting(const std::string& name, CameraDeviceSettingValue* value);

    std::string name_;
    std::unique_ptr<CameraDeviceSettingValue> value_;
};

/**
 * @brief This class is base for value of camera settings.
 */
class RCSDK_API CameraDeviceSettingValue {
public:
    virtual ~CameraDeviceSettingValue() = default;

    virtual bool operator==(const CameraDeviceSettingValue& obj) const = 0;
    virtual bool operator!=(const CameraDeviceSettingValue& obj) const = 0;

    virtual std::string toString() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_DEVICE_SETTING_HPP_
