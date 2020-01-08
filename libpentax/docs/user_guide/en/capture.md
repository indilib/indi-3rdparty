---
layout: docs
---

# Still Image Capture

This page shows how to capture still images with connected camera device using RICOH Camera USB SDK.

The RICOH Camera USB SDK supports single frame capture among still image capture.

### Contents

* [Related Classes](#related-classes)
* [How to Capture Still Images](#capture-still-images)
* [How to Focus and Capture Still Images](#focus-and-capture-still-images)
* [How to Check Capture Status](#check-capture-status)
* [How to Obtain Captured Image](#obtain-captured-image)
* [See Also](#see-also)

## Related Classes

The following classes are used when you capture images:

#### [`CameraImage`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html)

* The `CameraImage` class represents image information and image data.
* This class includes methods for obtaining image information and image data.

#### [`CameraEventListener`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html)

* The `CameraEventListener` class is a event listener which receives notifications from camera devices.

## Capture Still Images

Use the [`startCapture`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a0904d18bdc5034f39061d2755641b12a) method to release shutter of connected camera device.

```cpp
StartCaptureResponse startCaptureResponse = cameraDevice->startCapture();
if (startCaptureResponse.getResult() == Result::Ok) {
    std::cout << "Capturing StillImage has started. Capture ID: "
              << startCaptureResponse.getCapture()->getId()
              << std::endl;
} else {
    std::cout << "Capturing StillImage is FAILED. detail: "
              << startCaptureResponse.getErrors()[0]->getMessage()
              << std::endl;
}
```

## Focus and Capture Still Images

If you want to capture images after focusing, use the [`focus`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a1c2fec78bcbb891d1fa0df2268057dab) method and the [`startCapture`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a0904d18bdc5034f39061d2755641b12a) method with the `withFocus` argument `false`.

```cpp
Response response = cameraDevice->focus();
if (response.getResult() == Result::Ok) {
    std::cout << "Focus has started." << std::endl;
} else {
    std::cout << "Focus is FAILED. detail: "
              << response.getErrors()[0]->getMessage() << std::endl;
}
```

```cpp
bool withFocus = false;
StartCaptureResponse response = cameraDevice->startCapture(withFocus);
if (response.getResult() == Result::Ok) {
    std::cout << "Capturing StillImage without focus has started. Capture ID: "
              << response.getCapture()->getId() << std::endl;
} else {
    std::cout << "Capturing StillImage without focus is FAILED. detail: "
              << response.getErrors()[0]->getMessage() << std::endl;
}
```

## Check Capture Status

Use the [`getState`](../../api_reference/classRicoh_1_1CameraController_1_1Capture.html#aba2e71793eac9a0c970611655de5819c) method of the [`Capture`](../../api_reference/classRicoh_1_1CameraController_1_1Capture.html) class to check capture status. One of the supported values is `Complete`, which means the camera device has finished capturing.
You can obtain the `Capture` class with the three ways below:

1. Use the [`StartCaptureResponse`](../../api_reference/classRicoh_1_1CameraController_1_1StartCaptureResponse.html) class     
```cpp
StartCaptureResponse startCaptureResponse = cameraDevice->startCapture();
if (startCaptureResponse.getResult() == Result::Ok) {
    std::cout << "Capture state is "
              << startCaptureResponse.getCapture()->getState()
              << std::endl;
}
```

1. Use the [`CameraStatus`](../../api_reference/classRicoh_1_1CameraController_1_1CameraStatus.html) class     
```cpp
StartCaptureResponse startCaptureResponse = cameraDevice->startCapture();
if (startCaptureResponse.getResult() == Result::Ok) {
    std::cout << "Capture state is "
              << cameraDevice->getStatus().getCurrentCapture()->getState()
              << std::endl;
}
```

1. Use the [`captureComplete`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#acf6a221fa35224279cd59502516c3c8f) method of the [`CameraEventListener`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html) class     
```cpp
class UserEventListener : public CameraEventListener {
public:
    void captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                         const std::shared_ptr<const Capture>& capture) override
    {
        std::cout << "Capture Complete. Capture ID: " << capture->getId()
                  << std::endl;
    }
};
```

## Obtain Captured Image

Use the [`imageAdded`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#aa94febddd2a08a0f007e7a593cb64712) method of the [`CameraEventListener`](../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html) class to obtain the image you captured.

```cpp
class UserEventListener : public CameraEventListener {
public:
    void imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                    const std::shared_ptr<const CameraImage>& image) override
    {
        std::string filename = image->getName();
        std::ofstream o;
        o.open(filename, std::ofstream::out | std::ofstream::binary);

        Response res = image->getData(o);

        o.close();

        std::cout << "Get Image is "
                  << (res.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                  << std::endl;
    }
};
```

## See Also

* [Movie Capture](movie-capture.md) - Demonstrates how to capture movie images.
* [Image Handling](image-handling.md) - Demonstrates image acquisition.
* [Capture Settings](capture-settings.md) - Describes available capture settings such as capturing without saving data on camera storage.
* [Events](events.md) - Describes events such as `captureComplete` and `imageAdded`.
