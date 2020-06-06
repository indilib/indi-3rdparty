// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_SHUTTER_SPEED_HPP_
#define RICOH_CAMERA_SDK_SHUTTER_SPEED_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of shutter speed.
 */
class RCSDK_API ShutterSpeed : public CaptureSetting {
public:
    static const ShutterSpeed* SS1_24000;
    static const ShutterSpeed* SS1_20000;
    static const ShutterSpeed* SS1_16000;
    static const ShutterSpeed* SS1_12800;
    static const ShutterSpeed* SS1_12000;
    static const ShutterSpeed* SS1_10000;
    static const ShutterSpeed* SS1_8000;
    static const ShutterSpeed* SS1_6400;
    static const ShutterSpeed* SS1_6000;
    static const ShutterSpeed* SS1_5000;
    static const ShutterSpeed* SS1_4000;
    static const ShutterSpeed* SS1_3200;
    static const ShutterSpeed* SS1_3000;
    static const ShutterSpeed* SS1_2500;
    static const ShutterSpeed* SS1_2000;
    static const ShutterSpeed* SS1_1600;
    static const ShutterSpeed* SS1_1500;
    static const ShutterSpeed* SS1_1250;
    static const ShutterSpeed* SS1_1000;
    static const ShutterSpeed* SS1_800;
    static const ShutterSpeed* SS1_750;
    static const ShutterSpeed* SS1_640;
    static const ShutterSpeed* SS1_500;
    static const ShutterSpeed* SS1_400;
    static const ShutterSpeed* SS1_350;
    static const ShutterSpeed* SS1_320;
    static const ShutterSpeed* SS1_250;
    static const ShutterSpeed* SS1_200;
    static const ShutterSpeed* SS1_180;
    static const ShutterSpeed* SS1_160;
    static const ShutterSpeed* SS1_125;
    static const ShutterSpeed* SS1_100;
    static const ShutterSpeed* SS1_90;
    static const ShutterSpeed* SS1_80;
    static const ShutterSpeed* SS1_60;
    static const ShutterSpeed* SS1_50;
    static const ShutterSpeed* SS1_45;
    static const ShutterSpeed* SS1_40;
    static const ShutterSpeed* SS1_30;
    static const ShutterSpeed* SS1_25;
    static const ShutterSpeed* SS1_20;
    static const ShutterSpeed* SS1_15;
    static const ShutterSpeed* SS1_13;
    static const ShutterSpeed* SS1_10;
    static const ShutterSpeed* SS1_8;
    static const ShutterSpeed* SS1_6;
    static const ShutterSpeed* SS1_5;
    static const ShutterSpeed* SS1_4;
    static const ShutterSpeed* SS1_3;
    static const ShutterSpeed* SS3_10;
    static const ShutterSpeed* SS10_25;
    static const ShutterSpeed* SS4_10;
    static const ShutterSpeed* SS1_2;
    static const ShutterSpeed* SS5_10;
    static const ShutterSpeed* SS6_10;
    static const ShutterSpeed* SS10_16;
    static const ShutterSpeed* SS7_10;
    static const ShutterSpeed* SS10_13;
    static const ShutterSpeed* SS8_10;
    static const ShutterSpeed* SS1;
    static const ShutterSpeed* SS13_10;
    static const ShutterSpeed* SS15_10;
    static const ShutterSpeed* SS16_10;
    static const ShutterSpeed* SS2;
    static const ShutterSpeed* SS25_10;
    static const ShutterSpeed* SS3;
    static const ShutterSpeed* SS32_10;
    static const ShutterSpeed* SS4;
    static const ShutterSpeed* SS5;
    static const ShutterSpeed* SS6;
    static const ShutterSpeed* SS8;
    static const ShutterSpeed* SS10;
    static const ShutterSpeed* SS13;
    static const ShutterSpeed* SS15;
    static const ShutterSpeed* SS20;
    static const ShutterSpeed* SS25;
    static const ShutterSpeed* SS30;
    static const ShutterSpeed* SS40;
    static const ShutterSpeed* SS50;
    static const ShutterSpeed* SS60;
    static const ShutterSpeed* SS70;
    static const ShutterSpeed* SS80;
    static const ShutterSpeed* SS90;
    static const ShutterSpeed* SS100;
    static const ShutterSpeed* SS110;
    static const ShutterSpeed* SS120;
    static const ShutterSpeed* SS130;
    static const ShutterSpeed* SS140;
    static const ShutterSpeed* SS150;
    static const ShutterSpeed* SS160;
    static const ShutterSpeed* SS170;
    static const ShutterSpeed* SS180;
    static const ShutterSpeed* SS190;
    static const ShutterSpeed* SS200;
    static const ShutterSpeed* SS210;
    static const ShutterSpeed* SS220;
    static const ShutterSpeed* SS230;
    static const ShutterSpeed* SS240;
    static const ShutterSpeed* SS250;
    static const ShutterSpeed* SS260;
    static const ShutterSpeed* SS270;
    static const ShutterSpeed* SS280;
    static const ShutterSpeed* SS290;
    static const ShutterSpeed* SS300;
    static const ShutterSpeed* SS360;
    static const ShutterSpeed* SS420;
    static const ShutterSpeed* SS480;
    static const ShutterSpeed* SS540;
    static const ShutterSpeed* SS600;
    static const ShutterSpeed* SS660;
    static const ShutterSpeed* SS720;
    static const ShutterSpeed* SS780;
    static const ShutterSpeed* SS840;
    static const ShutterSpeed* SS900;
    static const ShutterSpeed* SS960;
    static const ShutterSpeed* SS1020;
    static const ShutterSpeed* SS1080;
    static const ShutterSpeed* SS1140;
    static const ShutterSpeed* SS1200;
    static const ShutterSpeed* Bulb;
    static const ShutterSpeed* Auto;

    /**
     * @brief Constructs an object not having a value.
     */
    ShutterSpeed();

    ~ShutterSpeed() = default;

private:
    ShutterSpeed(const std::string& speed);

    static const std::string Name;
};

/**
 * @brief The class for setting value of shutter speed.
 */
class RCSDK_API ShutterSpeedValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    ShutterSpeedValue(const std::string& value);

    ~ShutterSpeedValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_SHUTTER_SPEED_HPP_
