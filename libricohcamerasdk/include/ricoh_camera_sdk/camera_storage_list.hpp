// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_STORAGE_LIST_HPP_
#define RICOH_CAMERA_SDK_CAMERA_STORAGE_LIST_HPP_

#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraStorage;

/**
 * @brief The list of CameraStorage.
 */
class RCSDK_API CameraStorageList {
public:
    virtual ~CameraStorageList() = default;

    /**
     * @brief Returns CameraStorage of the specified index.
     *
     * @param index The index in list.
     * @return Camera storage
     */
    virtual std::shared_ptr<const CameraStorage> get(size_t index) const = 0;

    /**
     * @brief Returns the number of elements.
     *
     * @return The number of elements
     */
    virtual size_t size() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_STORAGE_LIST_HPP_
