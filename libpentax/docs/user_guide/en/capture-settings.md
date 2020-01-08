---
layout: docs
---

# Capture Settings

This page describes how to set and get various capture settings of connected camera device using RICOH Camera USB SDK. Note that if you set invalid value, rounded value might be set.

### Contents

* [Core Class](#core-class)
* [Basic Usage](#basic-usage)

**Supported Capture Settings**

* [Aperture Value](#aperture-value)
* [Exposure Compensation](#exposure-compensation)
* [ISO Sensitivity Value](#iso-sensitivity-value)
* [White Balance](#white-balance)
* [Shutter Speed](#shutter-speed)
* [Storage Settings](#storage-settings)
* [Still Image Quality](#still-image-quality)
* [Still Image Capture Format](#still-image-capture-format)
* [Capture Method](#capture-method)
* [Exposure Program](#exposure-program)
* [Hyper Operation Settings](#hyper-operation-settings)
* [User Capture Settings Mode](#user-capture-settings-mode)

## Core Class

The core class of capture settings is:

#### [`CaptureSetting`](../../api_reference/classRicoh_1_1CameraController_1_1CaptureSetting.html)

* The `CaptureSetting` class represents various capture settings of camera devices.
* Supported capture settings are provided as derived classes of the `CaptureSetting` class.

## Basic Usage

All the supported capture settings have corresponding classes derived from the [`CaptureSetting`](../../api_reference/classRicoh_1_1CameraController_1_1CaptureSetting.html) class. This section shows you how to use manage individual capture settings using the classes. You can also manage multiple capture settings at the same time. Code examples in this section show how to manage aperture value but you can manage other settings in similar ways.

### Get Individual Capture Settings

Follow the steps below to get current value of specified capture setting.

1. Generate an object of the class corresponding to the capture setting you want
1. Use the [`getCaptureSettings`][getCaptureSettings] method with the object

The following example shows how to get current aperture value using `FNumber` object.

```cpp
std::unique_ptr<FNumber> fNumber(new FNumber());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{fNumber.get()});
std::cout << "Current Value: " << fNumber->toString() << std::endl;
```

### List Available Setting Values

Follow the steps below to get a list of available values for individual capture settings.

1. Generate an object of the class corresponding to the capture setting you want
1. Use the [`getCaptureSettings`][getCaptureSettings] method with the object
1. Use the [`getAvailableSettings`][getAvailableSettings] method of the object

The following example shows how to get a list of available aperture values using `FNumber` object.

```cpp
std::unique_ptr<FNumber> fNumber(new FNumber());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{fNumber.get()});
const std::vector<const CaptureSetting*>& availableFNumberSettings =
    fNumber->getAvailableSettings();
for (auto setting : availableFNumberSettings) {
    std::cout << "Available Setting: " << setting->toString() << std::endl;
}
```

### Set Individual Capture Setting

Follow the steps below to set value of specified capture setting.

1. Select a class and its field corresponding to the value you want to set
1. Use the [`setCaptureSettings`][setCaptureSettings] method with the field

The following example shows how to set the aperture value to 5.6 using `FNumber` object.

```cpp
const FNumber* fNumber = FNumber::F5_6;
cameraDevice->setCaptureSettings(std::vector<const CaptureSetting*>{fNumber});
```

### Get Multiple Capture Settings

The following example shows how to get shutter speed and exposure compensation value at the same time.

```cpp
std::unique_ptr<ShutterSpeed> shutterSpeed(new ShutterSpeed());
std::unique_ptr<ExposureCompensation> exposureCompensation(
    new ExposureCompensation());
cameraDevice->getCaptureSettings(
    std::vector<CaptureSetting*>{
        shutterSpeed.get(), exposureCompensation.get()});
```

### Set Multiple Capture Settings

The following example shows how to set shutter speed and exposure compensation value at the same time.

```cpp
const ShutterSpeed* shutterSpeed = ShutterSpeed::SS1_10;
const ExposureCompensation* exposureCompensation = ExposureCompensation::EC2_0;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{shutterSpeed, exposureCompensation});
```

## Aperture Value

Use the [`FNumber`](../../api_reference/classRicoh_1_1CameraController_1_1FNumber.html) class to set and get aperture values.

### Get the Current Value & List Available Values

The following example shows how to get current aperture value using `FNumber` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `FNumber` class to get a list of available aperture values.

```cpp
std::unique_ptr<FNumber> fNumber(new FNumber());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{fNumber.get()});
const std::vector<const CaptureSetting*>& availableFNumberSettings =
    fNumber->getAvailableSettings();

// The list above might contain the following values.
// F4.0 (F4_0), F4.5 (F4_5), F5.0 (F5_0)
```

Available setting values vary depending on conditions such as lens and focal length of the camera.

### Set Value

The following example demonstrates how to set aperture value to 5.6 using `FNumber` object.

```cpp
const FNumber* fNumber = FNumber::F5_6;
cameraDevice->setCaptureSettings(std::vector<const CaptureSetting*>{fNumber});
```

## Exposure Compensation

Use the [`ExposureCompensation`](../../api_reference/classRicoh_1_1CameraController_1_1ExposureCompensation.html) class to set and get exposure compensation values.

### Get the Current Value & List Available Values

The following example shows how to get current exposure compensation value using `ExposureCompensation` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `ExposureCompensation` class to get a list of available exposure compensation value.

```cpp
std::unique_ptr<ExposureCompensation> exposureCompensation(
    new ExposureCompensation());
cameraDevice->getCaptureSettings(
    std::vector<CaptureSetting*>{exposureCompensation.get()});
const std::vector<const CaptureSetting*>& availableExposureCompensationSettings =
    exposureCompensation->getAvailableSettings();

// The list above might contain the following values
// in case you set EV step to 1/3.
// -5EV (ECNegative5_0), -4.7EV (ECNegative4_7), -4.3EV (ECNegative4_3),
// ...
// -1EV (ECNegative1_0), -0.7EV (ECNegative0_7), -0.3EV (ECNegative0_3),
// 0 (EC0_0),
// +0.3EV (EC0_3), +0.7EV (EC0_7), +1EV (EC1_0),
// ...
// +4.3EV (EC4_3), +4.7EV (EC4_7), +5EV (EC5_0)
```

### Set Value

The following example demonstrates how to set exposure compensation to +0.3EV using `ExposureCompensation` object.

```cpp
const ExposureCompensation* exposureCompensation = ExposureCompensation::EC0_3;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{exposureCompensation});
```

## ISO Sensitivity Value

Use the [`ISO`](../../api_reference/classRicoh_1_1CameraController_1_1ISO.html) class to set and get ISO sensitivity values.

### Get the Current Value & List Available Values

The following example shows how to get current ISO sensitivity value using `ISO` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `ISO` class to get a list of available ISO sensitivity values.

```cpp
std::unique_ptr<ISO> iso(new ISO());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{iso.get()});
const std::vector<const CaptureSetting*>& availableISOSettings =
    iso->getAvailableSettings();

// The list above might contain the following values
// in case you set program automatic exposure mode.
// automatic ISO setting (Auto),
// 100 (ISO100), 200 (ISO200), ... 102400 (ISO102400), 204800 (ISO204800)
```

Available setting values vary depending on conditions such as exposure mode of the camera.

### Set Value

The following example demonstrates how to set ISO sensitivity value to ISO400 using `ISO` object.

```cpp
const ISO* iso = ISO::ISO400;
cameraDevice->setCaptureSettings(std::vector<const CaptureSetting*>{iso});
```

## White Balance

Use the [`WhiteBalance`](../../api_reference/classRicoh_1_1CameraController_1_1WhiteBalance.html) class to set and get white balance values.

### Get the Current Value & List Available Values

The following example shows how to get current white balance value using `WhiteBalance` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `WhiteBalance` class to get a list of available white balance values.

```cpp
std::unique_ptr<WhiteBalance> whiteBalance(new WhiteBalance());
cameraDevice->getCaptureSettings(
    std::vector<CaptureSetting*>{whiteBalance.get()});
const std::vector<const CaptureSetting*>& availableWhiteBalanceSettings =
    whiteBalance->getAvailableSettings();

// The list above might contain the following values.
// Auto White Balance (Auto), Daylight (Daylight), Cloudy (Cloud),
// ...
// Manual White Balance Setting 1 (Manual),
// Manual White Balance Setting 2 (Manual2),
// Manual White Balance Setting 3 (Manual3)
```

### Set Value

The following example demonstrates how to set white balance to daylight using `WhiteBalance` object.

```cpp
const WhiteBalance* whiteBalance = WhiteBalance::Daylight;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{whiteBalance});
```

## Shutter Speed

Use the [`ShutterSpeed`](../../api_reference/classRicoh_1_1CameraController_1_1ShutterSpeed.html) class to set and get shutter speed values.

### Get the Current Value & List Available Values

The following example shows how to get current shutter speed value using `ShutterSpeed` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `ShutterSpeed` class to get a list of available shutter speed values.

```cpp
std::unique_ptr<ShutterSpeed> shutterSpeed(new ShutterSpeed());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{shutterSpeed.get()});
const std::vector<const CaptureSetting*>& availableShutterSpeedSettings =
    shutterSpeed->getAvailableSettings();

// The list above might contain the following values
// in case you set shutter priority automatic exposure mode and set EV step to 1/3.
// 1/8000 (SS1_8000), 1/6400 (SS1_6400), ...
// 0.3" (SS3_10), 0.4" (SS4_10), ...
// 1" (SS1), 1.3" (SS13_10), ...
// 25" (SS25), 30" (SS30)
```

Available setting values vary depending on conditions such as exposure mode of the camera.

### Set Value

The following example demonstrates how to set shutter speed to 1/80 using `ShutterSpeed` object.

```cpp
const ShutterSpeed* shutterSpeed = ShutterSpeed::SS1_80;
cameraDevice->setCaptureSettings(std::vector<const CaptureSetting*>{shutterSpeed});
```

## Storage Settings

Use the [`StorageWriting`](../../api_reference/classRicoh_1_1CameraController_1_1StorageWriting.html) class to set and get storage settings. There are only 2 available values: to save and not to save shooting results.

### Get the Current Value & List Available Values

The following example shows how to get current storage setting value using `StorageWriting` object. Use the [`getAvailableSettings`][getAvailableSettings] property of the `StorageWriting` class to get a list of available storage setting values.

If you want to obtain a bool object from the acquired [`StorageWriting`](../../api_reference/classRicoh_1_1CameraController_1_1StorageWriting.html), use the [`get`](../../api_reference/classRicoh_1_1CameraController_1_1StorageWritingValue.html#ac8a441f800568f5f31dd29cc2fc230fc) method of [`StorageWritingValue`](../../api_reference/classRicoh_1_1CameraController_1_1StorageWritingValue.html), which stores the actual value in the [`StorageWriting`](../../api_reference/classRicoh_1_1CameraController_1_1StorageWriting.html) object.

```cpp
// save(true), not save(false)
std::unique_ptr<StorageWriting> storageWriting(new StorageWriting());
camera->getCaptureSettings(
    std::vector<CaptureSetting*>{storageWriting.get()});
const std::vector<const CaptureSetting*>& availableStorageWritingSettings =
    storageWriting->getAvailableSettings();
const StorageWritingValue& storageWritingValue =
    dynamic_cast<const StorageWritingValue&>(storageWriting->getValue());
bool isStorageWriting = storageWritingValue.get();
```

### Set Value

The following example demonstrates how to disable the saving option using `StorageWriting` object.

```cpp
const StorageWriting* storageWriting = StorageWriting::False;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{storageWriting});
```

## Still Image Quality

Use the [`StillImageQuality`](../../api_reference/classRicoh_1_1CameraController_1_1StillImageQuality.html) class to set and get still image quality values.

### Get the Current Value & List Available Values

The following example shows how to get current still image quality value using `StillImageQuality` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `StillImageQuality` class to get a list of available still image quality values.

```cpp
std::unique_ptr<StillImageQuality> stillImageQuality(new StillImageQuality());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{stillImageQuality.get()});
const std::vector<const CaptureSetting*>& availableStillImageQualitySettings =
    stillImageQuality->getAvailableSettings();

// The list above might contain the following values
// Large Image Size, Best Image Quality (LargeBest),
// Large Image Size, Better Image Quality (LargeBetter),
// Large Image Size, Good Image Quality (LargeGood),
// ...
// Extra Small Image Size, Good Image Quality (ExtraSmallGood)
```

### Set Value

The following example demonstrates how to set still image quality value to LargeBest using `StillImageQuality` object.

```cpp
const StillImageQuality* stillImageQuality = StillImageQuality::LargeBest;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{stillImageQuality});
```

## Still Image Capture Format

Use the [`StillImageCaptureFormat`](../../api_reference/classRicoh_1_1CameraController_1_1StillImageCaptureFormat.html) class to set and get still image capture format values.

### Get the Current Value & List Available Values

The following example shows how to get current still image capture format value using `StillImageCaptureFormat` object. Use the [`getAvailableSettings`][getAvailableSettings] method of the `StillImageCaptureFormat` class to get a list of available still image capture format values.

```cpp
std::unique_ptr<StillImageCaptureFormat> stillImageCaptureFormat(new StillImageCaptureFormat());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{stillImageCaptureFormat.get()});
const std::vector<const CaptureSetting*>& availableStillImageCaptureFormatSettings =
    stillImageCaptureFormat->getAvailableSettings();

// The list above might contain the following values
// JPEG, PEF, DNG , PEF and JPEG, ...
```

### Set Value

The following example demonstrates how to set still image capture format value to PEF using `StillImageCaptureFormat` object.

```cpp
const StillImageCaptureFormat* stillImageCaptureFormat = StillImageCaptureFormat::PEF;
cameraDevice->setCaptureSettings(
    std::vector<const CaptureSetting*>{stillImageCaptureFormat});
```

## Capture Method

Use the [`CaptureMethod`](../../api_reference/classRicoh_1_1CameraController_1_1CaptureMethod.html) class to get capture method values.
Capture method get only.

### Get the Current Value

The following example shows how to get current capture method value using `CaptureMethod` object.

```cpp
std::unique_ptr<CaptureMethod> captureMethod(new CaptureMethod());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{captureMethod.get()});

// Still Image, Movie
```

## Exposure Program

Use the [`ExposureProgram`](../../api_reference/classRicoh_1_1CameraController_1_1ExposureProgram.html) class to get exposure program values.
Exposure program get only.

### Get the Current Value

The following example shows how to get current exposure program value using `ExposureProgram` object.

```cpp
std::unique_ptr<ExposureProgram> exposureProgram(new ExposureProgram());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{exposureProgram.get()});

// The list above might contain the following values
// Auto, Program, ShutterSpeedPriority, ...
```

## Hyper Operation Settings

Use the [`HyperOperationEnable`](../../api_reference/classRicoh_1_1CameraController_1_1HyperOperationEnable.html) class to get hyper operation settings values.
Hyper operation settings get only.
There are only 2 available values: enable and disable.

### Get the Current Value

The following example shows how to get current hyper operation settings value using `HyperOperationEnable` object.

If you want to obtain a bool object from the acquired [`HyperOperationEnable`](../../api_reference/classRicoh_1_1CameraController_1_1HyperOperationEnable.html), use the [`get`](../../api_reference/classRicoh_1_1CameraController_1_1HyperOperationEnableValue.html#a301c55e469dc5fd7ad0df0941cea034a) method of [`HyperOperationEnableValue`](../../api_reference/classRicoh_1_1CameraController_1_1HyperOperationEnableValue.html), which stores the actual value in the [`HyperOperationEnable`](../../api_reference/classRicoh_1_1CameraController_1_1HyperOperationEnable.html) object.

```cpp
std::unique_ptr<HyperOperationEnable> hyperOperationEnable(new HyperOperationEnable());
camera->getCaptureSettings(
    std::vector<CaptureSetting*>{hyperOperationEnable.get()});
const std::vector<const CaptureSetting*>& availableHyperOperationEnableSettings =
    hyperOperationEnable->getAvailableSettings();
const HyperOperationEnableValue& hyperOperationEnableValue =
    dynamic_cast<const HyperOperationEnableValue&>(hyperOperationEnable->getValue());
bool isHyperOperationEnableValue = hyperOperationEnableValue.get();

// enable(true), disable(false)
```

## User Capture Settings Mode

Use the [`UserCaptureSettingsMode`](../../api_reference/classRicoh_1_1CameraController_1_1UserCaptureSettingsMode.html) class to get user capture settings mode values.
User capture settings mode get only.

### Get the Current Value

The following example shows how to get current user capture settings mode value using `UserCaptureSettingsMode` object.

```cpp
std::unique_ptr<UserCaptureSettingsMode> userCaptureSettingsMode(new UserCaptureSettingsMode());
cameraDevice->getCaptureSettings(std::vector<CaptureSetting*>{userCaptureSettingsMode.get()});

// The list above might contain the following values
// Unknown, None, User, User2, User3, ...
```

[getAvailableSettings]: ../../api_reference/classRicoh_1_1CameraController_1_1CaptureSetting.html#a37b3068ab7a8040ea0eab5be4b5f7dc8
[getCaptureSettings]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a69d7fa9c8dec2aa5ed366f9f8a082782
[setCaptureSettings]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a7bca29a6e6260d7e907d2e025051eda9
