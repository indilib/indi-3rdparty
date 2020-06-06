// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_STILL_IMAGE_QUALITY_HPP_
#define RICOH_CAMERA_SDK_STILL_IMAGE_QUALITY_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of recorded pixels and quality level.
 */
class RCSDK_API StillImageQuality : public CaptureSetting {
public:
    static const StillImageQuality* LargeBest;
    static const StillImageQuality* LargeBetter;
    static const StillImageQuality* LargeGood;
    static const StillImageQuality* MediumBest;
    static const StillImageQuality* MediumBetter;
    static const StillImageQuality* MediumGood;
    static const StillImageQuality* SmallBest;
    static const StillImageQuality* SmallBetter;
    static const StillImageQuality* SmallGood;
    static const StillImageQuality* ExtraSmallBest;
    static const StillImageQuality* ExtraSmallBetter;
    static const StillImageQuality* ExtraSmallGood;

    /**
     * @brief Constructs an object not having a value.
     */
    StillImageQuality();

    ~StillImageQuality() = default;

private:
    StillImageQuality(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of recorded pixels and quality level.
 */
class RCSDK_API StillImageQualityValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    StillImageQualityValue(const std::string& value);

    ~StillImageQualityValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_STILL_IMAGE_QUALITY_HPP_
