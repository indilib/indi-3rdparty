---
layout: docs
---

# Live View

This page shows how to obtain live view of connected camera devices using RICOH Camera USB SDK.

For devices that support to live view, please refer to [Appendix](appendix.md).

### Contents

* [Related Classes](#related-classes)
* [How to Set up Live View Event](#set-up-live-view-event)
* [How to Start Live View](#start-live-view)
* [How to Adjust Focus](#adjust-focus)
* [How to Focus at Specified Position](#focus-at-specified-position)
* [How to Capture with Focusing at Specified Position](#capture-with-focusing-at-specified-position)
* [How to Stop Live View](#stop-live-view)
* [See Also](#see-also)

## Related Classes

The following classes are used when you obtain live view:

#### [`CameraEventListener`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html)

* The `CameraEventListener` class is a event listener which receives notifications from camera devices.

#### [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html)

* The `CameraDevice` class represents a camera device.
* This class includes properties and methods for obtaining camera information and operating camera device.

## Set up Live View Event

To start live view, override the virtual method [`liveViewFrameUpdated`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#a48d27460ad315d21cc600c6a4ee7f2f8) included in the `CameraEventListener` class with the user-defined derived class. JPEG data is streamed all the time, which keeps calling `liveViewFrameUpdated`.

```cpp
class UserEventListener : public CameraEventListener {
public:
    void liveViewFrameUpdated(const std::shared_ptr<const CameraDevice>& sender,
                              const std::shared_ptr<const unsigned char>& liveViewFrame,
                              uint64_t frameSize) override
    {
        std::cout << "Live View Frame Updated. Frame Size: " << frameSize
                  << std::endl;
    }
};
```

## Start Live View

Use the [`startLiveView`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a486b1b7c924469c7f4dfdb620802a23b) to start live view.

```cpp
std::shared_ptr<CameraEventListener> userEventListener =
    std::make_shared<UserEventListener>();
cameraDevice->addEventListener(userEventListener);
Response response = cameraDevice->startLiveView();
```

## Adjust Focus

If you want to adjust the focus while checking the live view, use the [`focus`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#adbe2285756aecd72775ce2f090601f31) method with the `adjustment` argument.
Argument represents the moving direction and moving amount of the focus.
If the argument is positive, the focus moves forward. If the argument is negative, the focus moves backward.

```cpp
int adjustment = 100;
Response response = cameraDevice->focus(adjustment);
```

## Focus at Specified Position

If you want focus at specified position while checking the live view, use the [`focus`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a1017c60f0bfd8a143e2c8b8ec7405b3c) method with the `focusPoint` argument.

The position that can be specified varies depending on the devices and setting conditions, and information can be get from the [`Live View Specification`](camera-settings.md).

```cpp
Point focusPoint;
focusPoint.x = 0.2;
focusPoint.y = 0.5;
Response response = cameraDevice->focus(focusPoint);
```

## Capture with Focusing at Specified Position

If you want to capture with focusing at specified position while checking the live view, use the [`startCapture`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a881d4444f07d371ce3485ae4f0c902de) method with the `focusPoint` argument.

The position that can be specified varies depending on the devices and setting conditions, and information can be get from the [`Live View Specification`](camera-settings.md).

```cpp
Point focusPoint;
focusPoint.x = 0.2;
focusPoint.y = 0.5;
Response response = cameraDevice->startCapture(focusPoint);
```

## Stop Live View

Use the [`stopLiveView`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#acee9cdd9e02d6b4bf0f1a56de1727fb0) to stop live view.

```cpp
Response response = cameraDevice->stopLiveView();
```

## See Also

* [Camera Settings](camera-settings.md) - Describes camera settings.
* [Events](events.md) - Describes events such as `liveViewFrameUpdated`.
