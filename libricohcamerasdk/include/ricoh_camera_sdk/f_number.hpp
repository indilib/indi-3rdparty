// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_F_NUMBER_HPP_
#define RICOH_CAMERA_SDK_F_NUMBER_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of aperture value.
 */
class RCSDK_API FNumber : public CaptureSetting {
public:
    static const FNumber* Null;
    static const FNumber* F0_5;
    static const FNumber* F0_7;
    static const FNumber* F0_8;
    static const FNumber* F0_9;
    static const FNumber* F1_0;
    static const FNumber* F1_1;
    static const FNumber* F1_2;
    static const FNumber* F1_3;
    static const FNumber* F1_4;
    static const FNumber* F1_6;
    static const FNumber* F1_7;
    static const FNumber* F1_8;
    static const FNumber* F1_9;
    static const FNumber* F2_0;
    static const FNumber* F2_2;
    static const FNumber* F2_4;
    static const FNumber* F2_5;
    static const FNumber* F2_8;
    static const FNumber* F3_2;
    static const FNumber* F3_3;
    static const FNumber* F3_5;
    static const FNumber* F4_0;
    static const FNumber* F4_5;
    static const FNumber* F4_8;
    static const FNumber* F5_0;
    static const FNumber* F5_6;
    static const FNumber* F5_8;
    static const FNumber* F6_3;
    static const FNumber* F6_7;
    static const FNumber* F7_1;
    static const FNumber* F8_0;
    static const FNumber* F9_0;
    static const FNumber* F9_5;
    static const FNumber* F10;
    static const FNumber* F11;
    static const FNumber* F13;
    static const FNumber* F14;
    static const FNumber* F16;
    static const FNumber* F18;
    static const FNumber* F19;
    static const FNumber* F20;
    static const FNumber* F22;
    static const FNumber* F25;
    static const FNumber* F27;
    static const FNumber* F29;
    static const FNumber* F32;
    static const FNumber* F36;
    static const FNumber* F38;
    static const FNumber* F40;
    static const FNumber* F45;
    static const FNumber* F51;
    static const FNumber* F54;
    static const FNumber* F57;
    static const FNumber* F64;
    static const FNumber* F72;
    static const FNumber* F76;
    static const FNumber* F80;
    static const FNumber* F81;
    static const FNumber* F90;
    static const FNumber* F107;
    static const FNumber* F128;
    static const FNumber* F180;
    static const FNumber* F256;

    /**
     * @brief Constructs an object not having a value.
     */
    FNumber();

    ~FNumber() = default;

private:
    FNumber(const std::string& number);

    static const std::string Name;
};

/**
 * @brief The class for setting value of aperture value.
 */
class RCSDK_API FNumberValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    FNumberValue(const std::string& value);

    ~FNumberValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_F_NUMBER_HPP_
