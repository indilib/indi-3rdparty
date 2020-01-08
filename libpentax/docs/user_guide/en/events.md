---
layout: docs
---

# Events

You can receive camera events of the connected camera using the [`CameraEventListener`][CameraEventListener] class.

### Contents

* [Supported Camera Events](#supported-camera-events)
* [How to Register Events](#register-events)
* [Capture Settings Supported by CaptureSettingsChanged](#capture-settings-supported-by-capturesettingschanged)
* [See Also](#see-also)

## Supported Camera Events

RICOH Camera USB SDK supports the following camera events:

* [`imageAdded`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#aa94febddd2a08a0f007e7a593cb64712) is called when image is added to the camera. You can transfer image data before ImageStored is called.
* [`imageStored`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#a28146d16dada9adcb94c4fdae05858ae) is called when image is stored to the camera storage.
* [`captureComplete`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#acf6a221fa35224279cd59502516c3c8f) is called when capturing an image is completed.
* [`deviceDisconnected`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#acf6a221fa35224279cd59502516c3c8f) is called when the camera you've already made a connection is disconnected.
* [`liveViewFrameUpdated`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#a48d27460ad315d21cc600c6a4ee7f2f8) is called when live view frame data is updated.
* [`captureSettingsChanged`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#ab27d8ca59052f34724e272207401db53) is called when supported capture settings are changed.
 It does not be called if you change directly with [`setCaptureSettings`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a7bca29a6e6260d7e907d2e025051eda9) method.

## Register Events

Override the virtual method corresponding to the event you want to receive in your derived class of [`CameraEventListener`][CameraEventListener]. You can also see [Live View](live-view.md) for an example of `liveViewFrameUpdated`.

```cpp
class UserEventListener : public CameraEventListener {
public:
    void imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                    const std::shared_ptr<const CameraImage>& image) override
    {
        std::cout << "Image Added. Name: " << image->getName()
                  << ", Type: " << static_cast<int>(image->getType())
                  << ", Format: " << static_cast<int>(image->getFormat())
                  << ", Size: " << image->getSize()
                  << ", ID: " << image->getId() << std::endl;
    }

    void imageStored(const std::shared_ptr<const CameraDevice>& sender,
                     const std::shared_ptr<const CameraImage>& image) override
    {
        std::cout << "Image Stored. Name: " << image->getName()
                  << ", Type: " << static_cast<int>(image->getType())
                  << ", Format: " << static_cast<int>(image->getFormat())
                  << ", Size: " << image->getSize()
                  << ", ID: " << image->getId() << std::endl;
    }

    void captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                         const std::shared_ptr<const Capture>& capture) override
    {
        std::cout << "Capture Complete. Capture ID: " << capture->getId()
                  << std::endl;
    }

    void deviceDisconnected(const std::shared_ptr<const CameraDevice>& sender,
                            DeviceInterface inf) override
    {
        std::cout << "Device Disconnected." << std::endl;
    }

    void captureSettingsChanged(const std::shared_ptr<const CameraDevice>& sender,
                                const std::vector<std::shared_ptr<const CaptureSetting>>& newSettings) override
    {
        std::cout << "Capture Settings Changed." << std::endl;
        for (auto setting : newSettings) {
            std::cout << " " << setting->toString() << std::endl;
        }
    }
};
```

And register the event listener as follows:

```cpp
std::shared_ptr<CameraEventListener> userEventListener =
    std::make_shared<UserEventListener>();
cameraDevice->addEventListener(userEventListener);
```

Registered events are called when corresponding events occur at [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html).

## Capture Settings Supported by CaptureSettingsChanged

The [Capture Settings](capture-settings.md) to be notified are as follows.

* Aperture Value
* Exposure Compensation
* ISO Sensitivity Value
* White Balance
* Shutter Speed
* Exposure Program
* Hyper Operation Settings
* User Capture Settings Mode

## See Also

* [Still Image Capture](capture.md) - Demonstrates how to capture still images.
* [Connection](connection.md) - Demonstrates how to make connections with cameras.
* [Live View](live-view.md) - Describes `liveViewFrameUpdated`.
* [Capture Settings](capture-settings.md) - Describes available capture settings.

[CameraEventListener]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html
