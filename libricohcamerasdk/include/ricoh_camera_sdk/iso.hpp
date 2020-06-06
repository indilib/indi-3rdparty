// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_ISO_HPP_
#define RICOH_CAMERA_SDK_ISO_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of ISO sensitivity.
 */
class RCSDK_API ISO : public CaptureSetting {
public:
    static const ISO* Auto;
    static const ISO* ISO100;
    static const ISO* ISO125;
    static const ISO* ISO140;
    static const ISO* ISO160;
    static const ISO* ISO200;
    static const ISO* ISO250;
    static const ISO* ISO280;
    static const ISO* ISO320;
    static const ISO* ISO400;
    static const ISO* ISO500;
    static const ISO* ISO560;
    static const ISO* ISO640;
    static const ISO* ISO800;
    static const ISO* ISO1000;
    static const ISO* ISO1100;
    static const ISO* ISO1250;
    static const ISO* ISO1600;
    static const ISO* ISO2000;
    static const ISO* ISO2200;
    static const ISO* ISO2500;
    static const ISO* ISO3200;
    static const ISO* ISO4000;
    static const ISO* ISO4500;
    static const ISO* ISO5000;
    static const ISO* ISO6400;
    static const ISO* ISO8000;
    static const ISO* ISO9000;
    static const ISO* ISO10000;
    static const ISO* ISO12800;
    static const ISO* ISO16000;
    static const ISO* ISO18000;
    static const ISO* ISO20000;
    static const ISO* ISO25600;
    static const ISO* ISO32000;
    static const ISO* ISO36000;
    static const ISO* ISO40000;
    static const ISO* ISO51200;
    static const ISO* ISO64000;
    static const ISO* ISO72000;
    static const ISO* ISO80000;
    static const ISO* ISO102400;
    static const ISO* ISO128000;
    static const ISO* ISO144000;
    static const ISO* ISO160000;
    static const ISO* ISO204800;
    static const ISO* ISO256000;
    static const ISO* ISO288000;
    static const ISO* ISO320000;
    static const ISO* ISO409600;
    static const ISO* ISO512000;
    static const ISO* ISO576000;
    static const ISO* ISO640000;
    static const ISO* ISO819200;

    /**
     * @brief Constructs an object not having a value.
     */
    ISO();

    ~ISO() = default;

private:
    ISO(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of ISO sensitivity.
 */
class RCSDK_API ISOValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    ISOValue(const std::string& value);

    ~ISOValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_ISO_HPP_
