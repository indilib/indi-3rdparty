// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_LIVE_VIEW_SPECIFICATION_HPP_
#define RICOH_CAMERA_SDK_LIVE_VIEW_SPECIFICATION_HPP_

//#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/point.hpp>
#include <ricoh_camera_sdk/camera_device_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>
#include <array>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The stracture for live view image.
 */
struct LiveViewImage {
    int32_t width;
    int32_t height;

    /**
     * @brief The vertices of focusable rectangular area.
     * focusArea.at(0): upper-left vertex
     * focusArea.at(1): upper-right vertex
     * focusArea.at(2): lower-right vertex
     * focusArea.at(3): lower-left vertex
     */
    std::array<Point, 4>  focusArea;
};

/**
 * @brief The class for setting of live view specification.
 */
class RCSDK_API LiveViewSpecification : public CameraDeviceSetting {
public:

    /**
     * @brief Constructs an object not having a value.
     */
    LiveViewSpecification();

    ~LiveViewSpecification() = default;

private:
    LiveViewSpecification(const LiveViewImage& value);

    static const std::string Name;
};

/**
 * @brief The class for setting value of live view specification.
 */
class RCSDK_API LiveViewSpecificationValue : public CameraDeviceSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    LiveViewSpecificationValue(const LiveViewImage& value);

    ~LiveViewSpecificationValue() = default;

    bool operator==(const CameraDeviceSettingValue& obj) const override;
    bool operator!=(const CameraDeviceSettingValue& obj) const override;

    /**
     * @brief Returns the live view image information.
     *
     * @return live view image information.
     */
    const LiveViewImage& get() const;

    std::string toString() const override;

private:
    LiveViewImage value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_LIVE_VIEW_SPECIFICATION_HPP_
