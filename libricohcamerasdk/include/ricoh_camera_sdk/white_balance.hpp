// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_WHITE_BALANCE_HPP_
#define RICOH_CAMERA_SDK_WHITE_BALANCE_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of white balance.
 */
class RCSDK_API WhiteBalance : public CaptureSetting {
public:
    static const WhiteBalance* Undefined;
    static const WhiteBalance* Auto;
    static const WhiteBalance* Daylight;
    static const WhiteBalance* Tungsten;
    static const WhiteBalance* Flash;
    static const WhiteBalance* Shade;
    static const WhiteBalance* Cloudy;
    static const WhiteBalance* FluorescentDaylight;
    static const WhiteBalance* FluorescentDaylightWhite;
    static const WhiteBalance* FluorescentCoolWhite;
    static const WhiteBalance* FluorescentWarmWhite;
    static const WhiteBalance* Manual;
    static const WhiteBalance* Manual2;
    static const WhiteBalance* Manual3;
    static const WhiteBalance* ColorTemperature;
    static const WhiteBalance* ColorTemperature2;
    static const WhiteBalance* ColorTemperature3;
    static const WhiteBalance* CTE;
    static const WhiteBalance* MultiAuto;

    /**
     * @brief Constructs an object not having a value.
     */
    WhiteBalance();

    ~WhiteBalance() = default;

private:
    WhiteBalance(const std::string& name);

    static const std::string Name;
};

/**
 * @brief The class for setting value of white balance.
 */
class RCSDK_API WhiteBalanceValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    WhiteBalanceValue(const std::string& value);

    ~WhiteBalanceValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_WHITE_BALANCE_HPP_
