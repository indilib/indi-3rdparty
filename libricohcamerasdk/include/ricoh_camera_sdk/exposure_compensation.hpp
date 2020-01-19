// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_EXPOSURE_COMPENSATION_HPP_
#define RICOH_CAMERA_SDK_EXPOSURE_COMPENSATION_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of EV compensation.
 */
class RCSDK_API ExposureCompensation : public CaptureSetting {
public:
    static const ExposureCompensation* ECNegative5_0;
    static const ExposureCompensation* ECNegative4_7;
    static const ExposureCompensation* ECNegative4_5;
    static const ExposureCompensation* ECNegative4_3;
    static const ExposureCompensation* ECNegative4_0;
    static const ExposureCompensation* ECNegative3_7;
    static const ExposureCompensation* ECNegative3_5;
    static const ExposureCompensation* ECNegative3_3;
    static const ExposureCompensation* ECNegative3_0;
    static const ExposureCompensation* ECNegative2_7;
    static const ExposureCompensation* ECNegative2_5;
    static const ExposureCompensation* ECNegative2_3;
    static const ExposureCompensation* ECNegative2_0;
    static const ExposureCompensation* ECNegative1_7;
    static const ExposureCompensation* ECNegative1_5;
    static const ExposureCompensation* ECNegative1_3;
    static const ExposureCompensation* ECNegative1_0;
    static const ExposureCompensation* ECNegative0_7;
    static const ExposureCompensation* ECNegative0_5;
    static const ExposureCompensation* ECNegative0_3;
    static const ExposureCompensation* EC0_0;
    static const ExposureCompensation* EC0_3;
    static const ExposureCompensation* EC0_5;
    static const ExposureCompensation* EC0_7;
    static const ExposureCompensation* EC1_0;
    static const ExposureCompensation* EC1_3;
    static const ExposureCompensation* EC1_5;
    static const ExposureCompensation* EC1_7;
    static const ExposureCompensation* EC2_0;
    static const ExposureCompensation* EC2_3;
    static const ExposureCompensation* EC2_5;
    static const ExposureCompensation* EC2_7;
    static const ExposureCompensation* EC3_0;
    static const ExposureCompensation* EC3_3;
    static const ExposureCompensation* EC3_5;
    static const ExposureCompensation* EC3_7;
    static const ExposureCompensation* EC4_0;
    static const ExposureCompensation* EC4_3;
    static const ExposureCompensation* EC4_5;
    static const ExposureCompensation* EC4_7;
    static const ExposureCompensation* EC5_0;

    /**
     * @brief Constructs an object not having a value
     */
    ExposureCompensation();

    ~ExposureCompensation() = default;

private:
    ExposureCompensation(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of EV compensation.
 */
class RCSDK_API ExposureCompensationValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    ExposureCompensationValue(const std::string& value);

    ~ExposureCompensationValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_EXPOSURE_COMPENSATION_HPP_
