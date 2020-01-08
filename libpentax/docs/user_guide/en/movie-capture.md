---
layout: docs
---

# Movie Capture

This page shows how to capture movies with connected camera device using RICOH Camera USB SDK.

In order to capture movie, the following preparations are required.

* Insert the SD card in the camera body
* Operate the camera body to set the movie mode

For devices that support to capture movie, please refer to [Appendix](appendix.md).

### Contents

* [Related Classes](#related-classes)
* [How to Start Capture Movie](#start-capture-movie)
* [How to Stop Capture Movie](#stop-capture-movie)
* [See Also](#see-also)

## Related Classes

The following classes are used when you capture movie:

#### [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html)

* The `CameraDevice` class represents a camera device.
* This class includes methods for obtaining camera information and operating camera device.

## Start Capture Movie

Use the [`startCapture`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a0904d18bdc5034f39061d2755641b12a) method to start movie capturing.

```cpp
StartCaptureResponse startCaptureResponse = cameraDevice->startCapture();
if (startCaptureResponse.getResult() == Result::Ok) {
    std::cout << "Capturing Movie has started. Capture ID: "
              << startCaptureResponse.getCapture()->getId()
              << std::endl;
} else {
    std::cout << "Capturing Movie is FAILED. detail: "
              << startCaptureResponse.getErrors()[0]->getMessage()
              << std::endl;
}
```

## Stop Capture Movie

Use the [`stopCapture`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a87dce71be6290a523f498288c16dc227) method to stop movie capturing.

```cpp
Response response = cameraDevice->stopCapture();
std::cout << "StopCapture is "
          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
          << std::endl;
```

`How to Focus and Capture`, `How to Check Capture Status`, `How to Obtain Captured Image` process is the same as [Still Image Capture](capture.md).

## See Also

* [Still Image Capture](capture.md) - Demonstrates how to capture still images.
* [Image Handling](image-handling.md) - Demonstrates image acquisition.
* [Capture Settings](capture-settings.md) - Describes available capture settings.
* [Events](events.md) - Describes events such as `CaptureComplete` and `ImageAdded`.
