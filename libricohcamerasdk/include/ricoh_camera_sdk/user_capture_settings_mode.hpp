// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_USER_CAPTURE_SETTINGS_MODE_HPP_
#define RICOH_CAMERA_SDK_USER_CAPTURE_SETTINGS_MODE_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of user capture settings mode.
 */
class RCSDK_API UserCaptureSettingsMode : public CaptureSetting {
public:
    static const UserCaptureSettingsMode* None;
    static const UserCaptureSettingsMode* User;
    static const UserCaptureSettingsMode* User2;
    static const UserCaptureSettingsMode* User3;
    static const UserCaptureSettingsMode* User4;
    static const UserCaptureSettingsMode* User5;

    /**
     * @brief Constructs an object not having a value.
     */
    UserCaptureSettingsMode();

    ~UserCaptureSettingsMode() = default;

private:
    UserCaptureSettingsMode(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of user capture settings mode.
 */
class RCSDK_API UserCaptureSettingsModeValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    UserCaptureSettingsModeValue(const std::string& value);

    ~UserCaptureSettingsModeValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_USER_CAPTURE_SETTINGS_MODE_HPP_
