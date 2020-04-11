// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_IMAGE_HPP_
#define RICOH_CAMERA_SDK_CAMERA_IMAGE_HPP_

#include <string>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class Response;
class CameraStorage;

/**
 * @brief The file type.
 */
enum class ImageType {
    Unknown,
    StillImage,
    Movie
};

/**
 * @brief The file format.
 */
enum class ImageFormat {
    Unknown,
    JPEG,
    TIFF,
    DPOF,
    PEF,
    DNG,
    MP4,
    MOV,
    AVI
};

/**
 * @brief The class for handling with image information.
 */
class RCSDK_API CameraImage {
public:
    virtual ~CameraImage() = default;

    /**
     * @brief Returns the ID of the image.
     *
     * @return The ID
     */
    virtual const std::string& getId() const = 0;

    /**
     * @brief Returns the name of the image.
     *
     * @return The name
     */
    virtual const std::string& getName() const = 0;

    /**
     * @brief Returns the size of the image.
     *
     * @return The size
     */
    virtual uint64_t getSize() const = 0;

    /**
     * @brief Returns the type of the image.
     *
     * @return The type
     */
    virtual ImageType getType() const = 0;

    /**
     * @brief Returns the format of the image.
     *
     * @return The format
     */
    virtual ImageFormat getFormat() const = 0;

    /**
     * @brief Returns <code>true</code> if a thumbnail can be acquired.
     *
     * @return <code>true</code> If a thumbnail can be acquired and <code>false</code> otherwise
     */
    virtual bool hasThumbnail() const = 0;

    /**
     * @brief Returns the date and time of image creation.
     *
     * @return The date and time
     */
    virtual time_t getDateTime() const = 0;

    /**
     * @brief Returns <code>true</code> if the entity of image exists in device.
     *
     * @return <code>true</code> if the entity of image exists in device
     * and <code>false</code> otherwise
     */
    virtual bool isExist() const = 0;

    /**
     * @brief Returns the storage having the image
     *
     * @return The storage
     */
    virtual std::shared_ptr<const CameraStorage> getStorage() const = 0;

    /**
     * @brief Writes image data to a stream.
     *
     * @param outStream The written stream
     * @return The operation response
     */
    virtual Response getData(std::ostream& outStream) const = 0;

    /**
     * @brief Writes thumbnail data to a stream.
     *
     * @param outStream The written stream
     * @return The operation response
     */
    virtual Response getThumbnail(std::ostream& outStream) const = 0;

    /**
     * @brief Deletes image data on camera device.
     *
     * @return The operation response
     */
    virtual Response deleteData() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_IMAGE_HPP_
