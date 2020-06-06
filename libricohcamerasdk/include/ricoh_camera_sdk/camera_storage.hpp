// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_STORAGE_HPP_
#define RICOH_CAMERA_SDK_CAMERA_STORAGE_HPP_

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraImageList;

/**
 * @brief The storage type.
 */
enum class StorageType {
    Unknown,
    FixedROM,
    RemovableROM,
    FixedRAM,
    RemovableRAM
};

/**
 * @brief The storage permission.
 */
enum class StoragePermission {
    Unknown,
    ReadWrite,
    Read,
    ReadDelete
};

/**
 * @brief The type of acquisition status of image list in a storage.
 */
enum class StorageListImagesState {
    /**
     * An image list is not listed.
     * When starting acquisition, this state is changed to <code>Listing</code>.
     */
    NotListed,

    /**
     * An acquisition is in progress.
     * When finishing acquisition, this state is changed to <code>Listed</code>.
     */
    Listing,

    /**
     * An acquisition is pending.
     * When restarting acquisition, this state is changed to <code>Listing</code>.
     */
    Pending,

    /**
     * An image list is listed.
     * When reconnecting and starting acquisition, this state is changed to <code>Listing</code>.
     */
    Listed,

    /**
     * An image list is interrupted.
     * For example, a connection is disconnected when an acquisition is in progress.
     */
    Interrupted
};

/**
 * @brief The class for camera storage.
 */
class RCSDK_API CameraStorage {
public:
    virtual ~CameraStorage() = default;

    /**
     * @brief Returns the ID of the storage.
     *
     * @return The ID
     */
    virtual const std::string& getId() const = 0;

    /**
     * @brief Returns <code>true</code> if the storage is available.
     *
     * @return <code>true</code> If the storage is available and <code>false</code> otherwise
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Returns the type of the storage.
     *
     * @return The storage type
     */
    virtual StorageType getType() const = 0;

    /**
     * @brief Returns the permission of the storage.
     *
     * @return The storage permission
     */
    virtual StoragePermission getPermission() const = 0;

    /**
     * @brief Returns the max capacity of the storage.
     *
     * @return The max capacity of storage
     */
    virtual uint64_t getMaxCapacity() const = 0;

    /**
     * @brief Returns the free space of the storage in bytes.
     *
     * @return The free space of storage in bytes
     */
    virtual uint64_t getFreeSpace() const = 0;

    /**
     * @brief Returns the number of possible still image shots based on current settings.
     *
     * @return The number of possible still image shots
     */
    virtual uint32_t getRemainingPictures() const = 0;

    /**
     * @brief Returns the acquisition status of image list in the storage.
     *
     * @return The acquisition status of image list
     */
    virtual StorageListImagesState getListImagesState() const = 0;

    /**
     * @brief Returns the image list in the storage.
     *
     * @return The image list in the storage
     */
    virtual const CameraImageList& getImages() const = 0;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_STORAGE_HPP_
