---
layout: docs
permalink: /docs/camera-usb-sdk-cpp/
---

# RICOH Camera USB SDK for C++

RICOH Camera USB SDK enables your app to operate cameras. This page shows how to use SDK packages and a sample app, which comes with the SDK. Note that code examples in this document is written in C++11.

### Contents

* [Requirements](#requirements)
* [Quick Start](#quick-start)
* [Classes](#classes)
* [Versioning Rules](#versioning-rules)

## Requirements

### Supported Cameras

* PENTAX K-1 Mark II
* PENTAX KP
* PENTAX K-1
* PENTAX 645Z

Please set "USB connection" to "PTP" beforehand in camera side setting.

### Operating Environments

RICOH Camera USB SDK operate in Linux, Windows and macOS.  
We confirmed following environments to work properly.

* Linux
    * Ubuntu 16.04 (32bit, 64bit)
    * (Experimental) Raspberry Pi3 (Raspbian Stretch)
* Windows
    * Windows 7 (32bit, 64bit)
    * Windows 10 (32bit, 64bit)
* macOS
    * macOS 10.13 High Sierra

## Quick Start

Please refer to the following by your environment.

- [Quick Start for Linux](quick-start-linux.md)
- [Quick Start for Windows](quick-start-windows.md)
- [Quick Start for macOS](quick-start-mac.md)

## Classes

RICOH Camera SDK provides functions for camera operations such as shooting and acquiring images. For detailed class reference, visit [`C++ API Reference`](../../api_reference/index.html).

### CameraDevice Class

CameraDevice Class manages camera information and operates cameras. See [`CameraDevice Class in C++ API Reference`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html) for detailed class reference.

### CameraImage Class

CameraImage Class manages image information and image data. See [`CameraImage Class in C++ API Reference`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html) for detailed class reference.

## Versioning Rules

RICOH Camera USB SDK conforms to [Semantic Versioning](http://semver.org/spec/v2.0.0.html). See below for versioning rules of the SDK.

### Versioning Syntax

RICOH Camera USB SDK is versioned in accordance with the following syntax:

{MAJOR}.{MINOR}.{PATCH}

Field Name  |Description
------------|---
MAJOR       |Major Version is<br>incremented when some changes that don't have backwards-compatibility with functionality occur.
MINOR       |Minor Version is<br>incremented when some interfaces are changed or added while keep having backwards-compatibility with functionality.<br>In this case, some build errors and other compatible issues may occur in your existing applications.
PATCH       |Patch Version is<br>incremented when some security fixes and bug fixes are made while keep having backwards-compatibility with functionality.<br>In this case, existing application build and compatibility with functionality is guaranteed.

