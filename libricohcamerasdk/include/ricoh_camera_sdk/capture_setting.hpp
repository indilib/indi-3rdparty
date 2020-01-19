// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAPTURE_SETTING_HPP_
#define RICOH_CAMERA_SDK_CAPTURE_SETTING_HPP_

#include <string>
#include <vector>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CaptureSettingValue;

/**
 * @brief This class is base class for capture settings.
 * Sub classes of this class are used to handle capture settings.
 */
class RCSDK_API CaptureSetting {
public:
    virtual ~CaptureSetting() = default;

    virtual bool operator==(const CaptureSetting& obj) const;
    virtual bool operator!=(const CaptureSetting& obj) const;

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
    virtual const CaptureSettingValue& getValue() const;

    /**
     * @brief Sets the value of a setting
     *
     * @param value The value of a setting
     */
    virtual void setValue(std::unique_ptr<CaptureSettingValue>&& value);

    /**
     * @brief Returns the valid settings that varies depending on the state and setting of
     * the camera device
     *
     * @return The valid settings
     */
    virtual const std::vector<const CaptureSetting*>& getAvailableSettings() const;

    virtual std::string toString() const;

protected:
    CaptureSetting(const std::string& name);
    CaptureSetting(const std::string& name, CaptureSettingValue* value);

    std::string name_;
    std::unique_ptr<CaptureSettingValue> value_;
    std::vector<const CaptureSetting*> availableSettings_;
};

/**
 * @brief This class is base for value of capture settings.
 */
class RCSDK_API CaptureSettingValue {
public:
    virtual ~CaptureSettingValue() = default;

    virtual bool operator==(const CaptureSettingValue& obj) const = 0;
    virtual bool operator!=(const CaptureSettingValue& obj) const = 0;

    virtual std::string toString() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAPTURE_SETTING_HPP_
