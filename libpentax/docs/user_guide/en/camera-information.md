---
layout: docs
---

# Camera Information

This page shows you how to obtain camera information using RICOH Camera USB SDK.

### Contents

* [Related Classes](#related-classes)
* [How to Obtain Camera Infomation](#obtain-camera-information)
* [How to Obtain Camera Status](#obtain-camera-status)

## Related Classes

The following classes are used when you obtain camera information:

#### [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html)

* The `CameraDevice` class represents a camera device.
* This class includes methods for obtaining camera information and operating camera device.

#### [`CameraStatus`](../../api_reference/classRicoh_1_1CameraController_1_1CameraStatus.html)

* The `CameraStatus` class represents camera status.

## Obtain Camera Information

Use `CameraDevice`'s method to obtain camera information such as model name and firmware version.

```cpp
// Manufacturer Name
std::string manufacturer = cameraDevice->getManufacturer();
// Model Name
std::string model = cameraDevice->getModel();
// Firmware Version
std::string firmwareVersion = cameraDevice->getFirmwareVersion();
// Serial Number
std::string serialNumber = cameraDevice->getSerialNumber();
```

## Obtain Camera Status

Use `CameraStatus`'s method to obtain camera status such as charge level.

```cpp
// Battery Level(%)
//     100(FULL), 67, 33, 0
uint32_t batteryLevel = cameraDevice->getStatus().getBatteryLevel();
```
