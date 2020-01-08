---
layout: docs
---

# Quick Start for Windows

This page shows how to start RICOH Camera USB SDK on Windows.

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

The executable files are built on Visual Studio 2015.
Therefore the executable files require Visual C++ 2015 Runtime.
If your environment does not has the runtime, please install [Visual C++ Redistributable for Visual Studio 2015](https://www.microsoft.com/en-US/download/details.aspx?id=48145).

The executable files are under `samples/bin` as `*.exe`.

### Running the app you build from the source code

To build the sample application, you need [Visual Studio](https://www.visualstudio.com/). This contents is confirmed in Visual Studio Express 2015 for Windows Desktop Update 3.

Solution files (`*.sln`) are under `samples/*` except `samples/bin`. Please open `*.sln` and build solution. A built executable file is created under `samples/*/build`.

**Note:** Live view sample (`samples/liveview/liveview.sln`) can not be built in Visual Studio 2017 under the influence of [wxWidgets](https://www.wxwidgets.org/).

## Integrate SDK into Your App

RICOH Camera USB SDK is provided in the shared library, `lib/*/camera-usb-sdk-cpp.dll` and `lib/*/camera-usb-sdk-cpp.lib`. Please link when building your application.

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
