---
layout: docs
---

# Quick Start for Linux

This page shows how to start RICOH Camera USB SDK on Linux.

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

Live view sample app is required libwxgtk3.0-dev.

```shell_session
sudo apt-get update
sudo apt-get install libwxgtk3.0-dev
```

### Running the executable file

#### cli sample

```shell_session
LD_LIBRARY_PATH=lib/(x64|x86|armv7l) samples/bin/(x64|x86|armv7l)/cli
```

`(x64|x86|armv7l)` should be selected according to the environment.

#### live view sample

```shell_session
LD_LIBRARY_PATH=lib/(x64|x86|armv7l) samples/bin/(x64|x86|armv7l)/liveview
```

### Running the app you build from the source code

To build the sample application, you need:

* GNU GCC 5.1 or later
* CMake 2.8.2 or later

Installation command example.

```shell_session
sudo apt-get install build-essential
sudo apt-get install cmake
```


#### cli sample

```shell_session
cd samples/cli
mkdir build
cd build
cmake ..
make
LD_LIBRARY_PATH=../../../lib/(x64|x86|armv7l) ./cli
```

#### live view sample

```shell_session
cd samples/liveview
mkdir build
cd build
cmake ..
make
LD_LIBRARY_PATH=../../../lib/(x64|x86|armv7l) ./liveview
```

## Integrate SDK into Your App

RICOH Camera USB SDK is provided in the shared library, `lib/(x64|x86|armv7l)/libRicohCameraSDKCpp.so`. Please link when building your application.

The header file is in `include` directory. Please set include path at build time and include `ricoh_camera_sdk.hpp` in your source code.

```cpp
#include "ricoh_camera_sdk.hpp"
using namespace Ricoh::CameraController;
```

At runtime you need `lib/(x64|x86|armv7l)/libRicohCameraSDKCpp.so` and `lib/(x64|x86|armv7l)/libmtp.so*`.

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
