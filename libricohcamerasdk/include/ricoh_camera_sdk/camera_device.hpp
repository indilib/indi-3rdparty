// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_DEVICE_HPP_
#define RICOH_CAMERA_SDK_CAMERA_DEVICE_HPP_

#include <vector>
#include <map>
#include <memory>

#include <ricoh_camera_sdk/camera_status.hpp>
#include <ricoh_camera_sdk/point.hpp>
#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class Response;
class StartCaptureResponse;
class CameraStorageList;
class CameraImageList;
class CameraEventListener;
class CameraDeviceSetting;
class CaptureSetting;

/**
 * @brief The type of a interface for connection.
 */
enum class DeviceInterface {
    USB
};

/**
 * @brief The class for information and operations on a camera device.
 */
class RCSDK_API CameraDevice {
public:
    virtual ~CameraDevice() = default;

    virtual bool operator==(const CameraDevice& obj) const = 0;
    virtual bool operator!=(const CameraDevice& obj) const = 0;

    /**
     * @brief Connects the camera device with a specified device interface.
     *
     * @param deviceInterface The interface for connection
     * @return The operation response
     */
    virtual Response connect(DeviceInterface deviceInterface) = 0;

    /**
     * @brief Disconnects the camera device with a specified device interface.
     *
     * @param deviceInterface The interface for connection
     * @return The operation response
     */
    virtual Response disconnect(DeviceInterface deviceInterface) = 0;

    /**
     * @brief Returns <code>true</code> if the camera device is connected by the interface.
     *
     * @param deviceInterface The interface for connection
     * @return <code>true</code> If the camera device is connected by the interface and
     * <code>false</code> otherwise
     */
    virtual bool isConnected(DeviceInterface deviceInterface) const = 0;

    /**
     * @brief Returns the manufacturer of the camera device.
     */
    virtual const std::string& getManufacturer() const = 0;

    /**
     * @brief Returns the model of the camera device.
     */
    virtual const std::string& getModel() const = 0;

    /**
     * @brief Returns the serial number of the camera device.
     */
    virtual const std::string& getFirmwareVersion() const = 0;

    /**
     * @brief Returns the serial number of the camera device.
     */
    virtual const std::string& getSerialNumber() const = 0;

    /**
     * @brief Returns the status of the camera device.
     *
     * @return The status
     */
    virtual const CameraStatus& getStatus() const = 0;

    /**
     * @brief Returns the storage list of the camera device.
     *
     * @return The storage list
     */
    virtual const CameraStorageList& getStorages() const = 0;

    /**
     * @brief Returns the image information list.
     *
     * Image list is updated automatically while connected in the background.
     *
     * @return The image list
     */
    virtual const CameraImageList& getImages() const = 0;

    /**
     * @brief Adds the specified event listener to the camera device.
     *
     * @param listener The event listener
     */
    virtual void addEventListener(const std::shared_ptr<CameraEventListener>& listener) = 0;

    /**
     * @brief Removes the specified event listener from the camera device.
     *
     * @param listener The event listener
     */
    virtual void removeEventListener(const std::shared_ptr<CameraEventListener>& listener) = 0;

    /**
     * @brief Returns the event listeners of the camera device.
     *
     * @return The event listeners
     */
    virtual const std::vector<std::shared_ptr<CameraEventListener>>& getEventListeners() const = 0;

    /**
     * @brief Performs auto focus.
     *
     * @return The operation response
     */
    virtual Response focus() = 0;

    /**
     * @brief Performs auto focus at specified focus point.
     *
     * @param point The focus point.
     * @return The operation response
     */
    virtual Response focus(const Point& point) = 0;

    /**
     * @brief Adjusts focus.
     *
     * @param adjustment The movement of image surface
     * @return The operation response
     */
    virtual Response focus(int adjustment) = 0;

     /**
     * @brief Starts a capture.
     *
     * @param withFocus <code>true</code> If capturing with doing auto-focus. Default value is true.
     * @return The operation response having the capture
     */
    virtual StartCaptureResponse startCapture(bool withFocus = true) = 0;

    /**
     * @brief Starts a capture with doing auto-focus at specified focus point.
     *
     * @param point The focus point.
     * @return The operation response having the capture
     */
    virtual StartCaptureResponse startCapture(const Point& point) = 0;

    /**
     * @brief Stop a capture.
     *
     * @return The operation response
     */
    virtual Response stopCapture() = 0;

   /**
    * @brief Starts a live view.
    *
    * @return The operation response
    */
    virtual Response startLiveView() = 0;

    /**
    * @brief Stops a live view.
    *
    * @return The operation response
    */
    virtual Response stopLiveView() = 0;

    /**
     * @brief Gets settings for camera device
     *
     * @param settings The settings
     * @return The operation response
     */
    virtual Response getCameraDeviceSettings(
        const std::vector<CameraDeviceSetting*>& settings) const = 0;

    /**
     * @brief Sets settings for camera device
     *
     * @param settings The settings
     * @return The operation response
     */
    virtual Response setCameraDeviceSettings(
        const std::vector<const CameraDeviceSetting*>& settings) const = 0;

    /**
     * @brief Gets settings for capture
     *
     * @param settings The settings
     * @return The operation response
     */
    virtual Response getCaptureSettings(
        const std::vector<CaptureSetting*>& settings) const = 0;

    /**
     * @brief Sets settings for capture
     *
     * @param settings The settings
     * @return The operation response
     */
    virtual Response setCaptureSettings(
        const std::vector<const CaptureSetting*>& settings) const = 0;

};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_DEVICE_HPP_
