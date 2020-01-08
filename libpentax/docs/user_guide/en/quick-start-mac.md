---
layout: docs
---

# Quick Start for macOS

This page shows how to start RICOH Camera USB SDK on macOS.

This page is designed to describe the two approaches to get started with RICOH Camera USB SDK. You can run Sample App to understand what the SDK can do and how it works. Also, you can add the SDK to your existing app to enhance it.

### Contents

* [Run Sample App](#run-sample-app)
* [Integrate SDK into Your App](#integrate-sdk-into-your-app)

## Run Sample App

Try out Sample App to understand RICOH Camera USB SDK quickly. Sample App has functions such as:

* Connecting to your camera
* Capturing images
* Changing camera settings

There are two options to run Sample App:

* Running the executable file
* Running the app you build from the source code

### Running the executable file

The executable files are under `samples/bin`.

### Running the app you build from the source code

To build the sample application, you need [Xcode](https://developer.apple.com/xcode/). This contents is confirmed in Xcode Version 9.2.

Xcode project files (`*.xcodeproj`) are under `samples/*` except `samples/bin`. Please open `*.xcodeproj` and build project. A built executable file is created under `samples/*/build`.

## Integrate SDK into Your App

RICOH Camera USB SDK is provided in the shared library, `lib/libcamera-usb-sdk-cpp.dylib`. Please link when building your application.

The header file is in `include` directory. Please set include path at build time and include `ricoh_camera_sdk.hpp` in your source code.

```cpp
#include "ricoh_camera_sdk.hpp"
using namespace Ricoh::CameraController;
```

Explore the quick references of each operation and events to enhance your app.

* [How to connect to cameras](connection.md)
* [How to get camera information](camera-information.md)
* [How to capture still images](capture.md)
* [How to capture movie](movie-capture.md)
* [How to handle images](image-handling.md)
* [How to obtain live view](live-view.md)
* [How to receive camera events](events.md)
* [Capture Settings](capture-settings.md)
* [Camera Settings](camera-settings.md)
