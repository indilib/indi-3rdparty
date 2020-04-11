// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_STORAGE_WRITING_HPP_
#define RICOH_CAMERA_SDK_STORAGE_WRITING_HPP_

#include <ricoh_camera_sdk/capture_setting.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The class for setting whether to save to storage.
 *
 * If no SD card is inserted, default setting is False and can not be changed to True.
 * Otherwise, default setting is True.
 */
class RCSDK_API StorageWriting : public CaptureSetting {
public:
    static const StorageWriting* True;
    static const StorageWriting* False;

    /**
     * @brief Constructs an object not having a value.
     */
    StorageWriting();

    /**
     * @brief Constructs an object having given a value.
     *
     * @param storageWriting whether to save to storage
     */
    StorageWriting(bool storageWriting);

    ~StorageWriting() = default;

private:

    static const std::string Name;
};

/**
 * @brief The class for setting value of storage writing.
 */
class RCSDK_API StorageWritingValue : public CaptureSettingValue {
public:
    /**
     * @brief Constructs an object having a value. You doesn't use this constructor.
     */
    StorageWritingValue(bool storageWriting);

    ~StorageWritingValue() = default;

    bool operator==(const CaptureSettingValue& obj) const override;
    bool operator!=(const CaptureSettingValue& obj) const override;

    /**
     * @brief Returns the setting value as bool.
     *
     * @return value wheter to save to storage.
     */
    bool get() const;

    std::string toString() const override;

private:
    bool value_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_STORAGE_WRITING_HPP_
