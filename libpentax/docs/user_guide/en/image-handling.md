---
layout: docs
---

# Image Handling

This page shows how to handle images in a connected camera storage using RICOH Camera USB SDK.

### Contents

* [Related Classes](#related-classes)
* [How to List Images](#list-images)
* [How to Obtain an Image](#obtain-an-image)
* [How to Obtain Image Thumbnails](#obtain-image-thumbnails)
* [How to Delete Images](#delete-images)

## Related Classes

The following classes are used when you manage images:

#### [`CameraImage`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html)

* The `CameraImage` class represents image information and image data.
* This class includes methods for obtaining image information and image data.

#### [`CameraImageList`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImageList.html)

* The `CameraImageList` class represents a list of `CameraImage`.

#### [`CameraDevice`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html)

* The `CameraDevice` class represents a camera device.
* This class includes methods for obtaining camera information and operating camera device.

#### [`CameraStorage`](../../api_reference/classRicoh_1_1CameraController_1_1CameraStorage.html)

* The `CameraStorage` class represents a storage of camera device.

#### [`CameraStorageList`](../../api_reference/classRicoh_1_1CameraController_1_1CameraStorageList.html)

* The `CameraStorageList` class represents a list of `CameraStorage`.

## List Images

Use the [`getImages`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a3a212c08711b3794386c5bf5c6dda1fb) method of the `CameraDevice` class to obtain a list of images stored in the connected camera device.

```cpp
for (size_t i = 0; i < camera->getStorages().size(); i++) {
    const auto& storage = camera->getStorages().get(i);
    std::cout << "[" << i << "]\n"
              << "  Storage Id: 0x"
              << std::hex << stoi(storage->getId()) << std::dec << std::endl;
    std::cout << "  StorageListImagesState: "
              << static_cast<int>(storage->getListImagesState()) << std::endl;
}
for (size_t i = 0; i < camera->getImages().size(); i++) {
    const auto& image = camera->getImages().get(i);
    std::cout << "  [" << i << "]"
              << " Storage ID: 0x" << std::hex
              << stoi(image->getStorage()->getId()) << std::dec
              << ", Name: " << image->getName()
              << ", Type: " << static_cast<int>(image->getType())
              << ", Format: " << static_cast<int>(image->getFormat())
              << ", Size: " << image->getSize()
              << ", ID: " << image->getId()
              << ", HasThumbnail: " << image->hasThumbnail()
              << ", Date: " << image->getDateTime() << std::endl;
}
```

The image list in camera storage is synchronized automatically in background process when the camera is connected and when the image list is updated. The image list returned by `getImages` is sorted in descending order of date and time. By using the [`getListImagesState`](../../api_reference/classRicoh_1_1CameraController_1_1CameraStorage.html#a887e11ced7f752ef1d493c866f1ea52e) method of the `CameraStorage` class, you can know acquisition status of the image list for each storage. Use the [`getStorages`](../../api_reference/classRicoh_1_1CameraController_1_1CameraDevice.html#a8593a3106b0237d8a58114f5d1381e56) method of the `CameraDevice` class to obtain a list of storages in the connected camera device.

**Note:** The image list is synchronized automatically in background. Therefore, a loop index of the image list may change while looping through. To obtain a snapshot of the image list, use the `clone` method of `CameraImageList` class.

```cpp
std::unique_ptr<const CameraImageList> images = cameraDevice->getImages().clone();
```

## Obtain an Image

Use `get` method of `CameraImageList` class to obtain a single image. And you can obtain image data by using [`getData`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html#a1c9a803f91c4e163d69b970c429db63e) method of `CameraImage` class.

```cpp
// Get the latest image
std::shared_ptr<const CameraImage> image = cameraDevice->getImages().get(0);
std::string filename = image->getName();
std::ofstream o;
o.open(filename, std::ofstream::out | std::ofstream::binary);

Response res = image->getData(o);

o.close();

std::cout << "Get Image is "
          << (res.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
          << std::endl;
if (res.getResult() == Result::Ok) {
    std::cout << "Image Path: " << filename << std::endl;
} else {
    for (const auto& error : res.getErrors()) {
        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                  << " / Error Message: " << error->getMessage() << std::endl;
    }
}
```

## Obtain Image Thumbnails

Use the [`getThumbnail`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html#a139feb818beb3823a34a6ddacfbfa85a) method to obtain image thumbnails.

```cpp
// Get the thumbnail of latest image
const std::shared_ptr<const CameraImage> image = cameraDevice->getImages().get(0);
std::string filename = "thumb_" + image->getName();
std::ofstream o;
o.open(filename, std::ofstream::out | std::ofstream::binary);

Response res = image->getThumbnail(o);

o.close();

std::cout << "Get Thumbnail Image is "
          << (res.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
          << std::endl;
if (res.getResult() == Result::Ok) {
    std::cout << "Image Path: " << filename << std::endl;
} else {
    for (const auto& error : res.getErrors()) {
        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                  << " / Error Message: " << error->getMessage() << std::endl;
    }
}
```

## Delete Images

Use the [`deleteData`](../../api_reference/classRicoh_1_1CameraController_1_1CameraImage.html#a0bf5ea471877eda227b91d7dfdc1b239) method to delete images in camera storage.

```cpp
// Delete the latest image
const std::shared_ptr<const CameraImage> image = cameraDevice->getImages().get(0);
Response res = image->deleteData();

std::cout << "Delete Image(" << image->getName() << ") is "
          << (res.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
          << std::endl;
```
