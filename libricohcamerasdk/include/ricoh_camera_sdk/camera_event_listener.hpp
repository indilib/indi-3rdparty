// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_CAMERA_EVENT_LISTENER_HPP_
#define RICOH_CAMERA_SDK_CAMERA_EVENT_LISTENER_HPP_

#include <memory>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

class CameraDevice;
class CameraImage;
class Capture;
enum class DeviceInterface;

/**
 * @brief The class of Event listener receiving notification from the camera device.
 */
class RCSDK_API CameraEventListener {
public:
    CameraEventListener(const CameraEventListener& rhs) = delete;
    virtual ~CameraEventListener() = default;

    CameraEventListener& operator=(const CameraEventListener& rhs) = delete;

    /**
     * @brief Invoked when a image was added to the camera device.
     *
     * You can transfer image data faster than stored CameraImage.
     *
     * @param sender The camera device sending this event
     * @param image  The added image
     */
    virtual void imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                            const std::shared_ptr<const CameraImage>& image) {}

    /**
     * @brief Invoked when a image was stored to a storage of the camera device.
     *
     * @param sender The camera device sending this event
     * @param image  The stored image
     */
    virtual void imageStored(const std::shared_ptr<const CameraDevice>& sender,
                             const std::shared_ptr<const CameraImage>& image) {}

    /**
     * @brief Invoked when a capture that has been started by this SDK was completed.
     *
     * @param sender  The camera device sending this event
     * @param capture The completed capture
     */
    virtual void captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                                 const std::shared_ptr<const Capture>& capture) {}

    /**
     * @brief Invoked when the connection was broken unexpectedly.

     * If call "disconnect()" method, this event is not invoked.
     *
     * @param sender The camera device sending this event
     * @param inf    The disconnected interface
     */
    virtual void deviceDisconnected(const std::shared_ptr<const CameraDevice>& sender,
                                    DeviceInterface inf) {}

    /**
     * @brief Invoked when a liveview image data was send.
     *
     * @param sender        The camera device sending this event
     * @param liveViewFrame The send live view image data
     * @param frameSize     The size of the send live view image data
     */
    virtual void liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender,
                                      const std::shared_ptr<const unsigned char>& liveViewFrame,
                                      uint64_t frameSize) {}

    /**
     * @brief Invoked when a capture setting was changed without being used setCaptureSettings method directly.
     *
     * @param sender       The camera device sending this event
     * @param newSettings  The changed settings
     */
    virtual void captureSettingsChanged(
        const std::shared_ptr<const CameraDevice>& sender,
        const std::vector<std::shared_ptr<const CaptureSetting>>& newSettings) {}

    protected:
    CameraEventListener() = default;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_CAMERA_EVENT_LISTENER_HPP_

