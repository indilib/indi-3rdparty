---
layout: docs
---

# Camera Settings

This page describes how to set and get various camera settings of connected camera device using RICOH Camera USB SDK.

### Contents

* [Core Class](#core-class)

**Supported Camera Settings**

* [Date and Time](#date-and-time)
* [Live View Specification](#live-view-specification)

## Core Class

The core class of camera settings is:

#### [`CameraDeviceSetting`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDeviceSetting.html)

* The `CameraDeviceSetting` class represents various camera settings of the connected camera device.
* Supported camera settings are provided as derived class of the `CameraDeviceSetting` class.


## Date and Time

Use the [`CameraTime`](../../api_reference/classRicoh_1_1CameraController_1_1CameraTime.html) class to set and get date and time of connected camera.

### Get the Current Value

The following example demonstrates how to get the current date and time of connected camera and `time_t` value of the same value.

```cpp
std::unique_ptr<CameraTime> cameraTime(new CameraTime());
cameraDevice->getCameraDeviceSettings(
    std::vector<CameraDeviceSetting*>{time.get()});
const CameraTimeValue& cameraTimeValue =
    dynamic_cast<const CameraTimeValue&>(cameraTime->getValue());
time_t cameraDateTime = cameraTimeValue.get();
```

If you want to obtain a `time_t` value from the acquired [`CameraTime`](../../api_reference/classRicoh_1_1CameraController_1_1CameraTime.html),
 use the [`get`](../../api_reference/classRicoh_1_1CameraController_1_1CameraTimeValue.html#a3571266533ae08ed8d45185beb296ec7)
 method of [`CameraTimeValue`](../../api_reference/classRicoh_1_1CameraController_1_1CameraTimeValue.html),
 which stores the actual value in the [`CameraTime`](../../api_reference/classRicoh_1_1CameraController_1_1CameraTime.html) object.


### Set Value

The following example demonstrates how to set camera device's date and time.

```cpp
time_t t = time(nullptr);
std::unique_ptr<const CameraTime> cameraTime(new CameraTime(t));
cameraDevice->setCameraDeviceSettings(
    std::vector<const CameraDeviceSetting*>{time.get()});
```

You can use the `CameraTime` class and the [`setCameraDeviceSettings`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a5432ceffffd23565e22caac5480745fc) method to set a specified `time_t` value.

Date and time of the camera device can be set by specifying `time_t` value for the constructor with arguments.

## Live View Specification

Use the [`LiveViewSpecification`](../../api_reference/classRicoh_1_1CameraController_1_1LiveViewSpecification.html) class to get current live view specification.

### Get Value

The following example demonstrates how to get live view specification.

```cpp
std::unique_ptr<LiveViewSpecification> liveViewSpecification(new LiveViewSpecification());
cameraDevice->getCameraDeviceSettings(
    std::vector<CameraDeviceSetting*>{liveViewSpecification.get()});
const LiveViewSpecificationValue& liveViewSpecificationValue =
    dynamic_cast<const LiveViewSpecificationValue&>(liveViewSpecification->getValue());
const LiveViewImage& liveViewImage = liveViewSpecificationValue.get();
// Image: 720x480,
// FocusArea:
//     (0.1, 0.166666), (0.9, 0.166666),
//     (0.1, 0.833333), (0.9, 0.833333)
```

If you want to obtain a [`LiveViewImage`](../../api_reference/structRicoh_1_1CameraController_1_1LiveViewImage.html) object
 from the acquired [`LiveViewSpecification`](../../api_reference/classRicoh_1_1CameraController_1_1LiveViewSpecification.html),
 use the [`get`](../../api_reference/classRicoh_1_1CameraController_1_1LiveViewSpecificationValue.html#a61700cbf407bd91087f5b00864a40937) method of
 [`LiveViewSpecificationValue`](../../api_reference/classRicoh_1_1CameraController_1_1LiveViewSpecificationValue.html),
 which stores the actual value in the [`LiveViewSpecification`](../../api_reference/classRicoh_1_1CameraController_1_1LiveViewSpecification.html) object.

[`LiveViewImage`](../../api_reference/structRicoh_1_1CameraController_1_1LiveViewImage.html) object has height and width size of the live view image and the range where focus position can be specified on the live view image.
[`focusArea`](../../api_reference/structRicoh_1_1CameraController_1_1LiveViewImage.html#aefd6ec71f7cb646c3de0f94b04559914) represents the upper left corner of the live view image as (0.0, 0.0), the upper right as (1.0, 0.0), the lower left as (0.0, 1.0), and the lower right as (1.0, 1.0).
The inside of the area represented by each point included in [`focusArea`](../../api_reference/structRicoh_1_1CameraController_1_1LiveViewImage.html#aefd6ec71f7cb646c3de0f94b04559914) is the position specifiable range.

This information is used by [`Focus with Specified Position`](live-view.md).
