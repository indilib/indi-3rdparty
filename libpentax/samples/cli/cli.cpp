// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#if defined(_WIN32)
#include <array>
#endif
#include <iostream>
#include <fstream>
#include <thread>
#include <cstdint>
#include <algorithm>
#include <future>
#include <vector>

#include <ricoh_camera_sdk.hpp>

using namespace Ricoh::CameraController;

static std::vector<std::shared_ptr<CameraDevice>> detectedCameraDevices;
static std::shared_ptr<CameraDevice> camera;

class EventListener : public CameraEventListener {
public:
    void imageAdded(const std::shared_ptr<const CameraDevice>& sender,
                    const std::shared_ptr<const CameraImage>& image) override
    {
        auto itr = std::find(detectedCameraDevices.begin(), detectedCameraDevices.end(), sender);
        std::cout << "\n[Device(" << (itr - detectedCameraDevices.begin()) << ")] Image Added. ";
        std::cout << "Name: " << image->getName()
                  << ", Type: " << static_cast<int>(image->getType())
                  << ", Format: " << static_cast<int>(image->getFormat())
                  << ", ID: " << image->getId() << std::endl;

        if (image->hasThumbnail()) {
            std::string filename = "thumb_" + image->getName();
            size_t extIdx = filename.find_last_of(".");
            filename.replace(extIdx, filename.length() - extIdx, ".JPG");
            std::ofstream o;
            o.open(filename, std::ofstream::out | std::ofstream::binary);
            Response response = image->getThumbnail(o);
            o.close();
            std::cout << "Get Thumbnail is "
                      << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                      << std::endl;
            if (response.getResult() == Result::Ok) {
                std::cout << "Image Path: " << filename << std::endl;
            } else {
                for (const auto& error : response.getErrors()) {
                    std::cout << "Error Code: " << static_cast<int>(error->getCode())
                              << " / Error Message: " << error->getMessage() << std::endl;
                }
            }
        }

        std::string filename = image->getName();
        std::ofstream o;
        o.open(filename, std::ofstream::out | std::ofstream::binary);
        Response response = image->getData(o);
        o.close();
        std::cout << "Get Image is "
                  << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                  << std::endl;
        if (response.getResult() == Result::Ok) {
            std::cout << "Image Path: " << filename << std::endl;
        } else {
            for (const auto& error : response.getErrors()) {
                std::cout << "Error Code: " << static_cast<int>(error->getCode())
                          << " / Error Message: " << error->getMessage() << std::endl;
            }
        }

        std::cout << "> " << std::flush;
    }

    void imageStored(const std::shared_ptr<const CameraDevice>& sender,
                     const std::shared_ptr<const CameraImage>& image) override
    {
        auto itr = std::find(detectedCameraDevices.begin(), detectedCameraDevices.end(), sender);
        std::cout << "\n[Device(" << (itr - detectedCameraDevices.begin()) << ")] Image Stored. ";
        std::cout << "Storage ID: 0x" << std::hex
                  << stoi(image->getStorage()->getId()) << std::dec
                  << ", Name: " << image->getName()
                  << ", Type: " << static_cast<int>(image->getType())
                  << ", Format: " << static_cast<int>(image->getFormat())
                  << ", Size: " << image->getSize()
                  << ", ID: " << image->getId()
                  << ", HasThumbnail: " << image->hasThumbnail()
                  << ", Date: " << image->getDateTime() << std::endl;
        std::cout << "> " << std::flush;
    }

    void captureComplete(const std::shared_ptr<const CameraDevice>& sender,
                         const std::shared_ptr<const Capture>& capture) override
    {
        auto itr = std::find(detectedCameraDevices.begin(), detectedCameraDevices.end(), sender);
        std::cout << "\n[Device(" << (itr - detectedCameraDevices.begin()) << ")] Capture Complete. "
                  << "Capture ID: " << capture->getId() << std::endl;
        std::cout << "> " << std::flush;
    }

    void deviceDisconnected(const std::shared_ptr<const CameraDevice>& sender,
                            DeviceInterface inf) override
    {
        auto itr = std::find(detectedCameraDevices.begin(), detectedCameraDevices.end(), sender);
        std::cout << "\n[Device(" << (itr - detectedCameraDevices.begin()) << ")] Disconnected."
                  << std::endl;
        std::cout << "> " << std::flush;
    }

    void captureSettingsChanged(const std::shared_ptr<const CameraDevice>& sender,
                                const std::vector<std::shared_ptr<const CaptureSetting>>& newSettings) override
    {
        auto itr = std::find(detectedCameraDevices.begin(), detectedCameraDevices.end(), sender);
        for (auto setting : newSettings) {
            std::cout << "\n[Device(" << (itr - detectedCameraDevices.begin()) << ")] CaptureSettingsChanged:";
            std::cout << " " << setting->toString() << std::endl;
        }
        std::cout << "> " << std::flush;
    }


};

void displaySelection()
{
    std::cout << std::endl;
    std::cout << "RICOH Camera USB SDK for C++ Sample Application" << std::endl;
    std::cout << "===============================================" << std::endl << std::endl;
    std::cout << "0.  Detect Device" << std::endl;
    std::cout << "1.  Connect Device" << std::endl;
    std::cout << "2.  Start Capture" << std::endl;
    std::cout << "3.  List Images" << std::endl;
    std::cout << "4.  Get Image" << std::endl;
    std::cout << "5.  Check Capture State" << std::endl;
    std::cout << "6.  Check Camera Status" << std::endl;
    std::cout << "7.  Check Storages" << std::endl;
    std::cout << "8.  Delete Image" << std::endl;
    std::cout << "9.  Get FNumber" << std::endl;
    std::cout << "10. Set FNumber" << std::endl;
    std::cout << "11. Get ExposureCompensation" << std::endl;
    std::cout << "12. Set ExposureCompensation" << std::endl;
    std::cout << "13. Get ISO" << std::endl;
    std::cout << "14. Set ISO" << std::endl;
    std::cout << "15. Get Thumbnail Image" << std::endl;
    std::cout << "16. Get White Balance" << std::endl;
    std::cout << "17. Set White Balance" << std::endl;
    std::cout << "18. Get Shutter Speed" << std::endl;
    std::cout << "19. Set Shutter Speed" << std::endl;
    std::cout << "20. Get Camera Time" << std::endl;
    std::cout << "21. Set Camera Time (Set Current Time)" << std::endl;
    std::cout << "22. Focus" << std::endl;
    std::cout << "23. Start Capture Without Focus" << std::endl;
    std::cout << "24. Get Storage Writing" << std::endl;
    std::cout << "25. Set Storage Writing" << std::endl;
    std::cout << "26. Connect All Devices" << std::endl;
    std::cout << "27. Focus of All Connecting Cameras" << std::endl;
    std::cout << "28. Start Capture Without Focus of All Connecting Cameras" << std::endl;
    std::cout << "29. Get ExposureProgram, HyperOperationEnable, UserCaptureSettingsMode" << std::endl;
    std::cout << "30. Stop Capture" << std::endl;
    std::cout << "31. Focus on Specified Position" << std::endl;
    std::cout << "32. Start Capture With Focusing on Specified Position" << std::endl;
    std::cout << "33. Get LiveView Specification" << std::endl;
    std::cout << "34. Get Still Image Capture Format" << std::endl;
    std::cout << "35. Set Still Image Capture Format" << std::endl;
    std::cout << "36. Get Still Image Quality" << std::endl;
    std::cout << "37. Set Still Image Quality" << std::endl;
    std::cout << "38. Get Capture Method" << std::endl;
    std::cout << "90. Disconnect Device" << std::endl;
    std::cout << "99. Exit" << std::endl;
    std::cout << "> " << std::flush;
}

void doMenu()
{
    uint32_t selectionIndex = 0;
    const bool selectionNeedDetect[] = {
        false,  false,  true,   true,   true,   true,   true,   true,   true,   true,   //  0 -  9
        true,   true,   true,   true,   true,   true,   true,   true,   true,   true,   // 10 - 19
        true,   true,   true,   true,   true,   true,   false,  false,  false,  true,   // 20 - 29
        true,   true,   true,   true,   true,   true,   true,   true,   true,   false,  // 30 - 39
        false,  false,  false,  false,  false,  false,  false,  false,  false,  true    // 40 - 49
    };

    std::vector<uint32_t> needDetected;
    for (uint32_t i = 0; i < 50; i++) {
        if (selectionNeedDetect[i]) {
            needDetected.push_back(i);
        }
    }

    std::cout << "RICOH Camera USB SDK for C++ Sample Application" << std::endl;
    std::cout << "Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved." << std::endl << std::endl;

    while (selectionIndex != 99) {
        displaySelection();
        std::string selectionIndexStr;
        std::getline(std::cin, selectionIndexStr);
        try {
            selectionIndex = stoi(selectionIndexStr);
        } catch (std::invalid_argument&) {
            continue;
        }
        bool isNeedDetected =
            std::any_of(needDetected.begin(), needDetected.end(), [selectionIndex](uint32_t num) {
                    return num == selectionIndex;
                });
        if (isNeedDetected && !camera) {
            std::cout << "Device was not found." << std::endl;
            continue;
        }

        switch (selectionIndex) {
            case 0: {
                detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
                size_t detectedCameraDevicesCount = detectedCameraDevices.size();
                if (detectedCameraDevicesCount > 0) {
                    std::cout << detectedCameraDevicesCount
                              << " Device(s) has been detected." << std::endl;
                    std::cout << "Detected Device(s):" << std::endl;
                    for (size_t i = 0; i < detectedCameraDevicesCount; i++) {
                        const auto& detectedCameraDevice = detectedCameraDevices[i];
                        std::cout << "  [" << i << "]" << std::endl;
                        std::cout << "    Manufacturer    : "
                                  << detectedCameraDevice->getManufacturer() << std::endl;
                        std::cout << "    Model           : "
                                  << detectedCameraDevice->getModel() << std::endl;
                        std::cout << "    Firmware Version: "
                                  << detectedCameraDevice->getFirmwareVersion() << std::endl;
                        std::cout << "    Serial Number   : "
                                  << detectedCameraDevice->getSerialNumber() << std::endl;
                        std::cout << "    USB Connection  : "
                                  << (detectedCameraDevice->isConnected(DeviceInterface::USB) ?
                                      "Connected" : "Disconnected") << std::endl;
                    }
                } else {
                    std::cout << "Device was not found." << std::endl;
                }
                break;
            }
            case 1: {
                detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
                if (detectedCameraDevices.size() > 0) {
                    camera = detectedCameraDevices[0];
                    if (camera->getEventListeners().size() == 0) {
                        std::shared_ptr<CameraEventListener> listener =
                            std::make_shared<EventListener>();
                        camera->addEventListener(listener);
                    }
                    Response response = camera->connect(DeviceInterface::USB);
                    std::cout << "Device connection is "
                              << (response.getResult() == Result::Ok ?
                                  "SUCCEED." : "FAILED.") << std::endl;
                    if (response.getResult() == Result::Ok) {
                        std::cout << "Connect Device:" << std::endl;
                        std::cout << "  Manufacturer    : "
                                  << camera->getManufacturer() << std::endl;
                        std::cout << "  Model           : "
                                  << camera->getModel() << std::endl;
                        std::cout << "  Firmware Version: "
                                  << camera->getFirmwareVersion() << std::endl;
                        std::cout << "  Serial Number   : "
                                  << camera->getSerialNumber() << std::endl;
                    } else {
                        for (const auto& error : response.getErrors()) {
                            std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                      << " / Error Message: " << error->getMessage() << std::endl;
                        }
                    }
                } else {
                    std::cout << "Device has not found." << std::endl;
                }
                break;
            }
            case 2: {
                try
                {
                    StartCaptureResponse response = camera->startCapture();
                    if (response.getResult() == Result::Ok) {
                        std::cout << "Capturing has started. Capture ID: "
                                  << response.getCapture()->getId() << std::endl;
                    } else {
                        std::cout << "Capturing is FAILED. detail: "
                                  << response.getErrors()[0]->getMessage() << std::endl;
                    }
                }catch (std::runtime_error e){
                    std::cout << "runtime_error : " << e.what() << std::endl;
                }
                break;
            }
            case 3: {
                std::cout << std::endl;
                for (size_t i = 0; i < camera->getStorages().size(); i++) {
                    const auto& storage = camera->getStorages().get(i);
                    std::cout << "[" << i << "]\n"
                              << "  Storage Id: 0x"
                              << std::hex << stoi(storage->getId()) << std::dec << std::endl;
                    std::cout << "  StorageListImagesState: "
                        << static_cast<int>(storage->getListImagesState()) << std::endl;
                    std::cout << "Images:" << std::endl;
                    for (size_t j = 0; j < storage->getImages().size(); j++) {
                        const auto& image = storage->getImages().get(j);
                        std::cout << "  [" << j << "]"
                                  << " Name: " << image->getName()
                                  << ", Type: " << static_cast<int>(image->getType())
                                  << ", Format: " << static_cast<int>(image->getFormat())
                                  << ", Size: " << image->getSize()
                                  << ", ID: " << image->getId()
                                  << ", HasThumbnail: " << image->hasThumbnail()
                                  << ", Date: " << image->getDateTime() << std::endl;
                    }
                    std::cout << std::endl;
                }
                std::cout << "AllImages:" << std::endl;
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
                break;
            }
            case 4: {
                std::cout << std::endl;
                std::cout << "Select Image[0-" << (camera->getImages().size() - 1) << "] > "
                          << std::flush;
                std::string imageIndexStr;
                int imageIndex = 0;
                std::getline(std::cin, imageIndexStr);
                try {
                    imageIndex = stoi(imageIndexStr);
                } catch (std::invalid_argument&) {
                    break;
                }
                auto image = camera->getImages().get(imageIndex);
                std::string filename = image->getName();
                std::ofstream o;
                o.open(filename, std::ofstream::out | std::ofstream::binary);
                Response response = image->getData(o);
                o.close();
                std::cout << "Get Image is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Ok) {
                    std::cout << "Image Path: " << filename << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 5: {
                auto capture = camera->getStatus().getCurrentCapture();
                if (!capture) {
                    std::cout << "Capture is NOT executing." << std::endl;
                } else {
                    std::cout << "Capture State:" << std::endl;
                    std::cout << "  ID    : " << capture->getId() << std::endl;
                    std::cout << "  Method: "
                              << capture->getMethod().getValue().toString() << std::endl;
                    std::cout << "  State : " << static_cast<int>(capture->getState()) << std::endl;
                }
                break;
            }
            case 6: {
                const auto& status = camera->getStatus();
                std::cout << "Camera status:" << std::endl;
                std::cout << "  BatteryLevel: " << status.getBatteryLevel() << std::endl;
                break;
            }
            case 7: {
                const auto& storages = camera->getStorages();
                std::cout << "Storages:" << std::endl;
                for (size_t i = 0; i < storages.size(); i++) {
                    const auto& storage = storages.get(i);
                    std::cout << "[" << i << "]" << std::endl;
                    std::cout << "  StorageID: 0x"
                              << std::hex << stoi(storage->getId()) << std::dec << std::endl;
                    std::cout << "    Type             : "
                        << static_cast<int>(storage->getType()) << std::endl;
                    std::cout << "    IsAvailable      : " << storage->isAvailable() << std::endl;
                    std::cout << "    MaxCapacity      : "
                              << storage->getMaxCapacity() << std::endl;
                    std::cout << "    Permission       : "
                        << static_cast<int>(storage->getPermission()) << std::endl;
                    std::cout << "    RemainingPictures: "
                              << storage->getRemainingPictures() << std::endl;
                    std::cout << "    FreeSpace        : " << storage->getFreeSpace() << std::endl;
                }
                break;
            }
            case 8: {
                std::cout << std::endl;
                std::cout << "Select Deleting Image[0-"
                          << (camera->getImages().size() - 1) << "] > " << std::flush;
                std::string imageIndexStr;
                int imageIndex = 0;
                std::getline(std::cin, imageIndexStr);
                try {
                    imageIndex = stoi(imageIndexStr);
                } catch (std::invalid_argument&) {
                    break;
                }
                auto image = camera->getImages().get(imageIndex);
                Response response = image->deleteData();
                std::cout << "Delete Image(" << image->getName() << ") is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                break;
            }
            case 9: {
                std::unique_ptr<FNumber> fNumber(new FNumber());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{fNumber.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << fNumber->toString() << std::endl;
                    std::cout << "FNumber Available:" << std::endl;
                    for (const auto fn : fNumber->getAvailableSettings()) {
                        std::cout << fn->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 10: {
                std::unique_ptr<FNumber> fNumber(new FNumber());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{fNumber.get()});
                std::cout << "FNumber [";
                for (const auto fn : fNumber->getAvailableSettings()) {
                    std::cout << fn->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputFNumber;
                std::getline(std::cin, inputFNumber);
                const CaptureSetting* setFNumber = nullptr;
                for (const auto fn : fNumber->getAvailableSettings()) {
                    if (fn->getValue().toString() == inputFNumber) {
                        setFNumber = fn;
                        break;
                    }
                }
                if (setFNumber == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setFNumber});
                std::cout << "FNumber setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 11: {
                std::unique_ptr<ExposureCompensation> exposureCompensation(
                    new ExposureCompensation());
                Response response =
                    camera->getCaptureSettings(
                        std::vector<CaptureSetting*>{exposureCompensation.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << exposureCompensation->toString() << std::endl;
                    std::cout << "ExposureCompensation Available:" << std::endl;
                    for (const auto ec : exposureCompensation->getAvailableSettings()) {
                        std::cout << ec->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 12: {
                std::unique_ptr<ExposureCompensation> exposureCompensation(
                    new ExposureCompensation());
                camera->getCaptureSettings(
                    std::vector<CaptureSetting*>{exposureCompensation.get()});
                std::cout << "ExposureCompensation [";
                for (const auto ec : exposureCompensation->getAvailableSettings()) {
                    std::cout << ec->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputExposureCompensation;
                std::getline(std::cin, inputExposureCompensation);
                const CaptureSetting* setExposureCompensation = nullptr;
                for (const auto ec : exposureCompensation->getAvailableSettings()) {
                    if (ec->getValue().toString() == inputExposureCompensation) {
                        setExposureCompensation = ec;
                        break;
                    }
                }
                if (setExposureCompensation == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setExposureCompensation});
                std::cout << "ExposureCompensation setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 13: {
                std::unique_ptr<ISO> iso(new ISO());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{iso.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << iso->toString() << std::endl;
                    std::cout << "ISO Available:" << std::endl;
                    for (const auto i : iso->getAvailableSettings()) {
                        std::cout << i->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 14: {
                std::unique_ptr<ISO> iso(new ISO());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{iso.get()});
                std::cout << "ISO [";
                for (const auto i : iso->getAvailableSettings()) {
                    std::cout << i->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputISO;
                std::getline(std::cin, inputISO);
                const CaptureSetting* setISO = nullptr;
                for (const auto i : iso->getAvailableSettings()) {
                    if (i->getValue().toString() == inputISO) {
                        setISO = i;
                        break;
                    }
                }
                if (setISO == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setISO});
                std::cout << "ISO setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 15: {
                std::cout << std::endl;
                std::cout << "Select Image[0-" << (camera->getImages().size() - 1) << "] > "
                          << std::flush;
                std::string imageIndexStr;
                int imageIndex = 0;
                std::getline(std::cin, imageIndexStr);
                try {
                    imageIndex = stoi(imageIndexStr);
                } catch (std::invalid_argument&) {
                    break;
                }
                auto image = camera->getImages().get(imageIndex);
                std::string filename = "thumb_" + image->getName();
                std::ofstream o;
                o.open(filename, std::ofstream::out | std::ofstream::binary);
                Response response = image->getThumbnail(o);
                o.close();
                std::cout << "Get Thumbnail Image is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Ok) {
                    std::cout << "Image Path: " << filename << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 16: {
                std::unique_ptr<WhiteBalance> whiteBalance(new WhiteBalance());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{whiteBalance.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << whiteBalance->toString() << std::endl;
                    std::cout << "WhiteBalance Available:" << std::endl;
                    for (const auto wb : whiteBalance->getAvailableSettings()) {
                        std::cout << wb->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 17: {
                std::unique_ptr<WhiteBalance> whiteBalance(new WhiteBalance());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{whiteBalance.get()});
                std::cout << "White Balance [";
                for (const auto wb : whiteBalance->getAvailableSettings()) {
                    std::cout << wb->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputWhiteBalance;
                std::getline(std::cin, inputWhiteBalance);
                const CaptureSetting* setWhiteBalance = nullptr;
                for (const auto wb : whiteBalance->getAvailableSettings()) {
                    if (wb->getValue().toString() == inputWhiteBalance) {
                        setWhiteBalance = wb;
                        break;
                    }
                }
                if (setWhiteBalance == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setWhiteBalance});
                std::cout << "White Balance setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 18: {
                std::unique_ptr<ShutterSpeed> shutterSpeed(new ShutterSpeed());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{shutterSpeed.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << shutterSpeed->toString() << std::endl;
                    std::cout << "ShutterSpeed Available:" << std::endl;
                    for (const auto ss : shutterSpeed->getAvailableSettings()) {
                        std::cout << ss->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 19: {
                std::unique_ptr<ShutterSpeed> shutterSpeed(new ShutterSpeed());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{shutterSpeed.get()});
                std::cout << "ShutterSpeed [";
                for (const auto ss : shutterSpeed->getAvailableSettings()) {
                    std::cout << ss->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputShutterSpeed;
                std::getline(std::cin, inputShutterSpeed);
                const CaptureSetting* setShutterSpeed = nullptr;
                for (const auto ss : shutterSpeed->getAvailableSettings()) {
                    if (ss->getValue().toString() == inputShutterSpeed) {
                        setShutterSpeed = ss;
                        break;
                    }
                }
                if (setShutterSpeed == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setShutterSpeed});
                std::cout << "ShutterSpeed setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 20: {
                std::unique_ptr<CameraTime> cameraTime(new CameraTime());
                Response response =
                    camera->getCameraDeviceSettings(
                        std::vector<CameraDeviceSetting*>{cameraTime.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << cameraTime->toString() << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 21: {
                time_t t = time(nullptr);
                std::unique_ptr<const CameraTime> setCameraTime(new CameraTime(t));
                Response response = camera->setCameraDeviceSettings(
                    std::vector<const CameraDeviceSetting*>{setCameraTime.get()});
                std::cout << "CameraTime setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 22: {
                Response response = camera->focus();
                if (response.getResult() == Result::Ok) {
                    std::cout << "Focus has started." << std::endl;
                } else {
                    std::cout << "Focus is FAILED. detail: "
                              << response.getErrors()[0]->getMessage() << std::endl;
                }
                break;
            }
            case 23: {
                try
                {
                    StartCaptureResponse response = camera->startCapture(false);
                    if (response.getResult() == Result::Ok) {
                        std::cout << "Capturing without focus has started. Capture ID: "
                                  << response.getCapture()->getId() << std::endl;
                    } else {
                        std::cout << "Capturing without focus is FAILED. detail: "
                                  << response.getErrors()[0]->getMessage() << std::endl;
                    }
                }catch (std::runtime_error e){
                    std::cout << "runtime_error : " << e.what() << std::endl;
                }
                break;
            }
            case 24: {
                std::unique_ptr<StorageWriting> storageWriting(new StorageWriting());
                camera->getCaptureSettings(
                    std::vector<CaptureSetting*>{storageWriting.get()});
                std::cout << storageWriting->toString() << std::endl;
                break;
            }
            case 25: {
                std::unique_ptr<StorageWriting> storageWriting(new StorageWriting());
                camera->getCaptureSettings(
                    std::vector<CaptureSetting*>{storageWriting.get()});
                std::cout << "StorageWriting [";
                for (const auto sw : storageWriting->getAvailableSettings()) {
                    std::cout << sw->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputStorageWriting;
                std::getline(std::cin, inputStorageWriting);
                bool isSave;
                if (inputStorageWriting == "true") {
                    isSave = true;
                } else if (inputStorageWriting == "false") {
                    isSave = false;
                } else {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                std::unique_ptr<const StorageWriting> setStorageWriting(new StorageWriting(isSave));
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setStorageWriting.get()});
                std::cout << "StorageWriting setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                break;
            }
            case 26: {
                detectedCameraDevices = CameraDeviceDetector::detect(DeviceInterface::USB);
                if ((!camera) && (detectedCameraDevices.size() > 0)) {
                    camera = detectedCameraDevices[0];
                }
                for (int i = 0; i < static_cast<int>(detectedCameraDevices.size()); i++) {
                    auto detectedCameraDevice = detectedCameraDevices[i];
                    if (detectedCameraDevice->getEventListeners().size() == 0) {
                        std::shared_ptr<CameraEventListener> listener =
                            std::make_shared<EventListener>();
                        detectedCameraDevice->addEventListener(listener);
                    }
                    Response response = detectedCameraDevice->connect(DeviceInterface::USB);
                    std::cout << "Connect Device(" << i << ") is "
                              << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                              << std::endl;
                }
                break;
            }
            case 27: {
                for (const auto& cameraDevice : detectedCameraDevices) {
                    if (cameraDevice->isConnected(DeviceInterface::USB)) {
                        cameraDevice->focus();
                    }
                }
                break;
            }
            case 28: {
                for (const auto& cameraDevice : detectedCameraDevices) {
                    std::vector<std::future<void>> captureTasks;
                    if (cameraDevice->isConnected(DeviceInterface::USB)) {
                        auto result = std::async(std::launch::async,
                                                 [cameraDevice]() {
                                                     cameraDevice->startCapture(false);
                                                 });
                        captureTasks.push_back(std::move(result));
                    }
                }
                break;
            }
            case 29: {
                std::unique_ptr<ExposureProgram> exposureProgram(new ExposureProgram());
                std::unique_ptr<HyperOperationEnable> hyperOperationEnable(new HyperOperationEnable());
                std::unique_ptr<UserCaptureSettingsMode> userCaptureSettingsMode(new UserCaptureSettingsMode());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{exposureProgram.get(),hyperOperationEnable.get(),userCaptureSettingsMode.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << exposureProgram->toString() << std::endl;
                    std::cout << hyperOperationEnable->toString() << std::endl;
                    std::cout << userCaptureSettingsMode->toString() << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 30: {
                Response response = camera->stopCapture();
                if (response.getResult() == Result::Ok) {
                    std::cout << "StopCapture is SUCCEED" << std::endl;
                } else {
                    std::cout << "StopCapture is FAILED. detail: "
                              << response.getErrors()[0]->getMessage() << std::endl;
                }
                break;
            }
            case 31: {
                float sx,ex,sy,ey;
                float x,y;
                std::unique_ptr<LiveViewSpecification> liveViewSpecification(new LiveViewSpecification());
                Response response =
                    camera->getCameraDeviceSettings(std::vector<CameraDeviceSetting*>{liveViewSpecification.get()});
                if (response.getResult() == Result::Ok) {
                    const LiveViewSpecificationValue& liveViewSpecificationValue =
                    dynamic_cast<const LiveViewSpecificationValue&>(liveViewSpecification->getValue());
                    LiveViewImage LiveViewValue = liveViewSpecificationValue.get();
                    std::cout << "FocusArea: (" << LiveViewValue.focusArea[0].x << "," << LiveViewValue.focusArea[0].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[1].x << "," << LiveViewValue.focusArea[1].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[2].x << "," << LiveViewValue.focusArea[2].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[3].x << "," << LiveViewValue.focusArea[3].y << ")"  << std::endl;
                    sx = LiveViewValue.focusArea[0].x;
                    ex = LiveViewValue.focusArea[1].x;
                    sy = LiveViewValue.focusArea[0].y;
                    ey = LiveViewValue.focusArea[3].y;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                std::cout << "[focus FocusPoint] x > " << std::flush;
                std::string inputFocusPointX;
                std::getline(std::cin, inputFocusPointX);
                try {
                    x = std::stof(inputFocusPointX);
                } catch (const std::invalid_argument) {
                    std::cout << "Invalid value " << std::endl;
                    break;
                }
                if((x >=  sx) && (x <=  ex)) {
                    std::cout << "[focus FocusPoint] y > " << std::flush;
                    std::string inputFocusPointY;
                    std::getline(std::cin, inputFocusPointY);
                    try {
                        y = std::stof(inputFocusPointY);
                    } catch (const std::invalid_argument) {
                        std::cout << "Invalid value " << std::endl;
                        break;
                    }
                    if((y >=  sy) && (y <=  ey)) {
                        std::cout << "Focus start. FocusPoint x="<< inputFocusPointX
                                << " y="<< inputFocusPointY << std::endl;
                        struct Point point;
                        point.x = x;
                        point.y = y;
                        Response response2 = camera->focus(point);
                        if (response2.getResult() == Result::Ok) {
                            std::cout << "Focus has started." << std::endl;
                        } else {
                            std::cout << "Focus is FAILED. detail: "
                                    << response2.getErrors()[0]->getMessage() << std::endl;
                        }
                    } else {
                        /* Y input error */
                        std::cout << "Invalid value " << std::endl;
                    }
                } else {
                    /* X input error */
                    std::cout << "Invalid value " << std::endl;
                }
                break;
            }
            case 32: {
                float sx,ex,sy,ey;
                float x,y;
                std::unique_ptr<LiveViewSpecification> liveViewSpecification(new LiveViewSpecification());
                Response response =
                    camera->getCameraDeviceSettings(std::vector<CameraDeviceSetting*>{liveViewSpecification.get()});
                if (response.getResult() == Result::Ok) {
                    const LiveViewSpecificationValue& liveViewSpecificationValue =
                    dynamic_cast<const LiveViewSpecificationValue&>(liveViewSpecification->getValue());
                    LiveViewImage LiveViewValue = liveViewSpecificationValue.get();
                    std::cout << "FocusArea: (" << LiveViewValue.focusArea[0].x << "," << LiveViewValue.focusArea[0].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[1].x << "," << LiveViewValue.focusArea[1].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[2].x << "," << LiveViewValue.focusArea[2].y << ")" ;
                    std::cout << " (" << LiveViewValue.focusArea[3].x << "," << LiveViewValue.focusArea[3].y << ")"  << std::endl;
                    sx = LiveViewValue.focusArea[0].x;
                    ex = LiveViewValue.focusArea[1].x;
                    sy = LiveViewValue.focusArea[0].y;
                    ey = LiveViewValue.focusArea[3].y;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                std::cout << "[focus FocusPoint] x > " << std::flush;
                std::string inputFocusPointX;
                std::getline(std::cin, inputFocusPointX);
                try {
                    x = std::stof(inputFocusPointX);
                } catch (const std::invalid_argument) {
                    std::cout << "Invalid value " << std::endl;
                    break;
                }
                if((x >=  sx) && (x <=  ex)) {
                    std::cout << "[focus FocusPoint] y > " << std::flush;
                    std::string inputFocusPointY;
                    std::getline(std::cin, inputFocusPointY);
                    try {
                        y = std::stof(inputFocusPointY);
                    } catch (const std::invalid_argument) {
                        std::cout << "Invalid value " << std::endl;
                        break;
                    }
                    if((y >=  sy) && (y <=  ey)) {
                        std::cout << "Capturing start. FocusPoint x="<< inputFocusPointX
                                << " y="<< inputFocusPointY << std::endl;
                        struct Point point;
                        point.x = x;
                        point.y = y;
                        StartCaptureResponse response2 = camera->startCapture(point);
                        if (response2.getResult() == Result::Ok) {
                            std::cout << "Capturing has started. Capture ID: "
                                    << response2.getCapture()->getId() << std::endl;
                        } else {
                            std::cout << "Capturing is FAILED. detail: "
                                    << response2.getErrors()[0]->getMessage() << std::endl;
                        }
                    } else {
                        /* Y input error */
                        std::cout << "Invalid value " << std::endl;
                    }
                } else {
                    /* X input error */
                    std::cout << "Invalid value " << std::endl;
                }
                break;
            }
            case 33: {
                std::unique_ptr<LiveViewSpecification> liveViewSpecification(new LiveViewSpecification());

                Response response =
                    camera->getCameraDeviceSettings(std::vector<CameraDeviceSetting*>{liveViewSpecification.get()});

                if (response.getResult() == Result::Ok) {
                    std::cout << liveViewSpecification->toString() << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 34: {
                std::unique_ptr<StillImageCaptureFormat> stillImageCaptureFormat(new StillImageCaptureFormat());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{stillImageCaptureFormat.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << stillImageCaptureFormat->toString() << std::endl;
                    std::cout << "StillImageCaptureFormat Available:" << std::endl;
                    for (const auto format : stillImageCaptureFormat->getAvailableSettings()) {
                        std::cout << format->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 35: {
                std::unique_ptr<StillImageCaptureFormat> stillImageCaptureFormat(new StillImageCaptureFormat());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{stillImageCaptureFormat.get()});
                std::cout << "StillImageCaptureFormat [";
                for (const auto fn : stillImageCaptureFormat->getAvailableSettings()) {
                    std::cout << fn->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputStillImageCaptureFormat;
                std::getline(std::cin, inputStillImageCaptureFormat);
                const CaptureSetting* setStillImageCaptureFormat = nullptr;
                for (const auto format : stillImageCaptureFormat->getAvailableSettings()) {
                    if (format->getValue().toString() == inputStillImageCaptureFormat) {
                        setStillImageCaptureFormat = format;
                        break;
                    }
                }
                if (setStillImageCaptureFormat == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setStillImageCaptureFormat});
                std::cout << "StillImageCaptureFormat setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }

                break;
            }
            case 36: {
                std::unique_ptr<StillImageQuality> stillImageQuality(new StillImageQuality());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{stillImageQuality.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << stillImageQuality->toString() << std::endl;
                    std::cout << "stillImageQuality Available:" << std::endl;
                    for (const auto quality : stillImageQuality->getAvailableSettings()) {
                        std::cout << quality->getValue().toString() << ", ";
                    }
                    std::cout << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 37: {
                std::unique_ptr<StillImageQuality> stillImageQuality(new StillImageQuality());
                camera->getCaptureSettings(std::vector<CaptureSetting*>{stillImageQuality.get()});
                std::cout << "StillImageQuality [";
                for (const auto quality : stillImageQuality->getAvailableSettings()) {
                    std::cout << quality->getValue().toString() << ", ";
                }
                std::cout << "] > " << std::flush;
                std::string inputStillImageQuality;
                std::getline(std::cin, inputStillImageQuality);
                const CaptureSetting* setStillImageQuality = nullptr;
                for (const auto quality : stillImageQuality->getAvailableSettings()) {
                    if (quality->getValue().toString() == inputStillImageQuality) {
                        setStillImageQuality = quality;
                        break;
                    }
                }
                if (setStillImageQuality == nullptr) {
                    std::cout << "Invalid value" << std::endl;
                    break;
                }
                Response response = camera->setCaptureSettings(
                    std::vector<const CaptureSetting*>{setStillImageQuality});
                std::cout << "StillImageQuality setting is "
                          << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                          << std::endl;
                if (response.getResult() == Result::Error) {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }

                break;

            }
            case 38: {
                std::unique_ptr<CaptureMethod> captureMethod(new CaptureMethod());
                Response response =
                    camera->getCaptureSettings(std::vector<CaptureSetting*>{captureMethod.get()});
                if (response.getResult() == Result::Ok) {
                    std::cout << captureMethod->toString() << std::endl;
                } else {
                    for (const auto& error : response.getErrors()) {
                        std::cout << "Error Code: " << static_cast<int>(error->getCode())
                                  << " / Error Message: " << error->getMessage() << std::endl;
                    }
                }
                break;
            }
            case 90: {
                for (int i = 0; i < static_cast<int>(detectedCameraDevices.size()); i++) {
                    auto detectedCameraDevice = detectedCameraDevices[i];
                    if (detectedCameraDevice->isConnected(DeviceInterface::USB)) {
                        Response response = detectedCameraDevice->disconnect(DeviceInterface::USB);
                        std::cout << "Disconnect Device(" << i << ") is "
                                  << (response.getResult() == Result::Ok ? "SUCCEED." : "FAILED.")
                                  << std::endl;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    if (detectedCameraDevices.size() > 0) {
        for (const auto& detectedCameraDevice : detectedCameraDevices) {
            if (detectedCameraDevice->isConnected(DeviceInterface::USB)) {
                detectedCameraDevice->disconnect(DeviceInterface::USB);
            }
        }
    }
}

int main()
{
    doMenu();

    return 0;
}
