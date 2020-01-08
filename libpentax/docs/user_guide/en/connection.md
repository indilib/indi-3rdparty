---
layout: docs
---

# Connection

This page shows you how to make connections with cameras using RICOH Camera USB SDK.

### Contents

* [Related Classes](#related-classes)
* [How to Detect Cameras](#detect-cameras)
* [How to Connect to Camera](#connect-to-camera)
* [How to Disconnect from Camera](#disconnect-from-camera)
* [How to Check Connection Status](#check-connection-status)
* [See Also](#see-also)

## Related Classes

The following classes are used when you make a connection with cameras:

#### [`CameraDeviceDetector`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDeviceDetector.html)

* The `CameraDeviceDetector` class includes methods for detecting camera devices which are connected with USBs.

#### [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html)

* The `CameraDevice` class represents a camera device.
* This class includes methods for obtaining camera information and operating camera device.

## Detect Cameras

Use the [`detect`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDeviceDetector.html#ad24a775ab711d3b254e717e672b74c94) to obtain a list of connectable camera devices.  
To connect to a camera, you need to detect connectable camera devices first.

```cpp
DeviceInterface deviceInterface = DeviceInterface::USB;
std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices =
    CameraDeviceDetector::detect(deviceInterface);
```

## Connect to Camera

Use the  [`connect`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#af58536abbbf2790fe66cd77586206ef6) method to connect to one of the detected camera devices.

```cpp
std::share_ptr<CameraDevice> cameraDevice = detectedCameraDevices[0];
Response response = cameraDevice->connect(deviceInterface);
```

## Disconnect from Camera

Use the [`disconnect`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a61c9562722a3a0df51fc5397b49b84ff) method to disconnect from the camera.

```cpp
Response response = cameraDevice->disconnect(deviceInterface);
```

## Check Connection Status

Use the [`isConnected`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#ae0df75ae1e9794798202040adaa8f939) method to check connection status of the camera.

```cpp
bool isConnected = cameraDevice->isConnected(deviceInterface);
```

## See Also

* [Events](events.md) - Describes events such as receiving camera event notifications.
