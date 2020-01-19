// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_EXPOSURE_PROGRAM_HPP_
#define RICOH_CAMERA_SDK_EXPOSURE_PROGRAM_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of exposure program.
 */
class RCSDK_API ExposureProgram : public CaptureSetting {
public:
    static const ExposureProgram* Unknown;
    static const ExposureProgram* Auto;
    static const ExposureProgram* Program;
    static const ExposureProgram* AperturePriority;
    static const ExposureProgram* ShutterSpeedPriority;
    static const ExposureProgram* ShutterSpeedAndAperturePriority;
    static const ExposureProgram* ISOPriority;
    static const ExposureProgram* Manual;
    static const ExposureProgram* Bulb;
    static const ExposureProgram* FlashXSyncSpeed;
    static const ExposureProgram* Astrotracer;
    static const ExposureProgram* LensShutter;
    
    /**
     * @brief Constructs an object not having a value.
     */
    ExposureProgram();

    ~ExposureProgram() = default;

private:
    ExposureProgram(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of exposure program.
 */
class RCSDK_API ExposureProgramValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    ExposureProgramValue(const std::string& value);

    ~ExposureProgramValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_EXPOSURE_PROGRAM_HPP_
