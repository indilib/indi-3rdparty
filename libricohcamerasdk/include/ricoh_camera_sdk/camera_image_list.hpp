// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_IMAGE_LIST_HPP_
#define RICOH_CAMERA_SDK_CAMERA_IMAGE_LIST_HPP_

#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraImage;

/**
 * @brief The list of CameraImage.
 */
class RCSDK_API CameraImageList {
public:
    virtual ~CameraImageList() = default;

    /**
     * @brief Returns CameraImage of the specified index.
     *
     * @param index The index in list.
     * @return Camera image
     */
    virtual std::shared_ptr<const CameraImage> get(size_t index) const = 0;

    /**
     * @brief Returns the number of elements.
     *
     * @return The number of elements
     */
    virtual size_t size() const = 0;

    /**
     * @brief Creates and returns a copy of image list.
     *
     * @return The copied image list
     */
    virtual std::unique_ptr<const CameraImageList> clone() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_IMAGE_LIST_HPP_
