// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_STILL_IMAGE_CAPTURE_FORMAT_HPP_
#define RICOH_CAMERA_SDK_STILL_IMAGE_CAPTURE_FORMAT_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting of recording formats.
 */
class RCSDK_API StillImageCaptureFormat : public CaptureSetting {
public:
    static const StillImageCaptureFormat* JPEG;
    static const StillImageCaptureFormat* PEF;
    static const StillImageCaptureFormat* DNG;
    static const StillImageCaptureFormat* PEFAndJPEG;
    static const StillImageCaptureFormat* DNGAndJPEG;
    static const StillImageCaptureFormat* TIFF;


    /**
     * @brief Constructs an object not having a value.
     */
    StillImageCaptureFormat();

    ~StillImageCaptureFormat() = default;

private:
    StillImageCaptureFormat(const std::string& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of recording formats.
 */
class RCSDK_API StillImageCaptureFormatValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    StillImageCaptureFormatValue(const std::string& value);

    ~StillImageCaptureFormatValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    std::string toString() const override;

private:
    std::string value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_STILL_IMAGE_CAPTURE_FORMAT_HPP_
