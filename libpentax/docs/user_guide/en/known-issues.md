---
layout: docs
---

# Known Issues

## Known Issues

The following table shows the known issues of RICOH Camera USB SDK.

Issue (Devices) | Solution
-- | --
The size etc. of [`CameraImage`][CameraImage] may not be acquired correctly (K-1 Mark II, KP, K-1, 645Z) | This issue may occur when capturing is performed continuously at short intervals under the conditions of the memory card option is set to "Save to Both" and the file format is set to RAW+. After waiting for the completion of the capturing process, please execute the next capture. Please refrain from playing back, editing, or deleting images on the camera body while connecting with the SDK.
In case of [`focus`][focus], the response is successful, but the autofocus is not executed (K-1 Mark II, KP, K-1, 645Z) | Please ensure the zooming index on the lens is set to a position that permits shutter release before calling this method.
In case of [`startCapture`][startCapture], the response is successful, but the capture process is not started (K-1 Mark II, KP, K-1, 645Z) | Please ensure the zooming index on the lens is set to a position that permits shutter release before calling this method.
If capturing is performed frequently and continuously without waiting for the previous capturing process, capturing may be impossible or capturing may not start even if [`startCapture`][startCapture] is successful (K-1 Mark II, KP, K-1, 645Z) | Please turn the power off and then on again. If the power can not be turned off, please remove the battery once and insert it again. After waiting for the completion of the capturing process, please execute the next capture.
If capturing is performed frequently and continuously without waiting for the previous capturing process, camera may freeze or reboot (K-1 Mark II, KP, K-1, 645Z) | Please turn the power off and then on again. If the power can not be turned off, please remove the battery once and insert it again. This issue may be avoided by using camera inserted no SD card.
Even when the shutter mode is the electronic shutter, only the shutter speed value at the mechanical shutter can be set (KP) | Please operate the camera body to set.
In rare cases, the camera freezes or turns off when starting or stopping to capture movie (K-1 Mark II, KP, K-1) | Please turn the power off and then on again. If the power can not be turned off, please remove the battery once and insert it again. In addition, illegal image files such as 0 byte MOV format files may be created when problems occur, and the same issue is likely to occur if movie images are taken with the file left. Please delete invalid file from SD card.
The card slot of the storage destination is not changed to the SD2 slot even when the memory card in the SD1 slot becomes full after capturing by using the SDK under the conditions of the memory card option is set to "Sequential Use" (K-1 Mark II, K-1, 645Z) | Please remove the SD card with no free space.
When resizing with operate the camera body, "Memory card error" message may be displayed (K-1 Mark II, KP, K-1, 645Z) | Please perform this operation while disconnecting the USB connection.
If you perform the following operation with the camera body, the camera may freeze or turn off : delete all images, image delete, resize, movie edit, playback mode (K-1 Mark II, KP, K-1, 645Z) | Please perform this operation while disconnecting the USB connection.
When switching from the still image mode to the movie mode on the camera body, rounding of the exposure compensation setting value is not reflected in the setting value (K-1 Mark II, KP, K-1) | Please set the available exposure compensation value again.
In movie mode, available list of shutter speed can not be acquired correctly (K-1 Mark II, KP, K-1) | Please operate the camera body to set.
The camera operation may not be performed correctly from the SDK when connect in Movie mode or after switching between movie mode and still image mode (K-1 Mark II, KP, K-1) | Please turn the power off and then on again.
If still image quality or still image capture format to set value in movie mode, it is not reflected (K-1 Mark II, K-1) | Please operate the camera body to set, or change to the still image mode and then make the setting from the SDK.
Camera may freeze when capturing under the conditions of the settings that two or more images are saved by one captureing, such as that the memory card options is set to "Save to Both" or the file format is set to RAW+. (645Z) | Please capture with settings that save one image by one capture.
If you capture continuously with the camera body, there are times when you can not receive the [`imageAdded`][imageAdded] event (645Z) | After waiting for the completion of the capturing process, please execute the next capture.
When the captured image is saved in a new folder, there are times when you can not receive the  [`imageAdded`][imageAdded], [`imageStored`][imageStored], [`captureComplete`][captureComplete] event. Also, the image list may not be able to synchronize normally (K-1 Mark II, KP, K-1, 645Z) | Please turn the power off and then on again. If the power can not be turned off, please remove the battery once and insert it again. This issue may be avoided by using camera inserted no SD card.
In rare cases, if capturing is performed frequently and continuously without waiting for the previous capturing process, events of [`imageAdded`][imageAdded], [`imageStored`][imageStored], [`captureComplete`][captureComplete] may not be received. Also, the image list may not be able to synchronize normally (K-1 Mark II, KP, K-1, 645Z) | Please turn the power off and then on again. If the power can not be turned off, please remove the battery once and insert it again. This issue may be avoided by using camera inserted no SD card.
In rare cases, the setting of aperture value, exposure compensation, shutter speed may not be reflected (KP) | Please set the available value again.
By using the SDK, the camera can not be set the following settings; ISO256000, ISO288000, ISO320000, ISO409600, ISO512000, ISO576000, ISO640000 and ISO819200 (K-1 Mark II) | Please operate the camera body to set.

[CameraImage]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html
[focus]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a1c2fec78bcbb891d1fa0df2268057dab
[startCapture]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a0904d18bdc5034f39061d2755641b12a
[imageAdded]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#aa94febddd2a08a0f007e7a593cb64712
[imageStored]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#a28146d16dada9adcb94c4fdae05858ae
[captureComplete]: ../../api_reference/classRicoh_1_1CameraController_1_1CameraEventListener.html#acf6a221fa35224279cd59502516c3c8f
