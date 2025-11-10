#include "TelescopeDataProcessor.hpp"
#include <QDateTime>
#include <QDebug>

TelescopeDataProcessor::TelescopeDataProcessor(QObject *parent) : QObject(parent) {
    // Initialize with default values
    reset();
}

void TelescopeDataProcessor::reset() {
    telescopeData = TelescopeData();
}

bool TelescopeDataProcessor::processJsonPacket(const QByteArray &jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        qDebug() << "Failed to parse JSON packet";
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    // Get common fields
    QString source = obj["Source"].toString();
    QString command = obj["Command"].toString();
    QString type = obj["Type"].toString();
    
    // Only process notifications
    if (type != "Notification" && type != "Response") {
        qDebug() << "Ignoring non-notification packet";
        return false;
    }
    
    // Route to appropriate handler based on source
    if (source == "Mount") {
        updateMountStatus(obj);
        emit mountStatusUpdated();
    }
    else if (source == "Camera" && command == "GetCaptureParameters") {
        updateCameraStatus(obj);
        emit cameraStatusUpdated();
    }
    else if (source == "Focuser") {
        updateFocuserStatus(obj);
        emit focuserStatusUpdated();
    }
    else if (source == "Environment") {
        updateEnvironmentStatus(obj);
        emit environmentStatusUpdated();
    }
    else if (source == "ImageServer" && command == "NewImageReady") {
        updateImageInfo(obj);
        emit newImageAvailable();
    }
    else if (source == "Disk") {
        updateDiskStatus(obj);
        emit diskStatusUpdated();
    }
    else if (source == "DewHeater") {
        updateDewHeaterStatus(obj);
        emit dewHeaterStatusUpdated();
    }
    else if (source == "OrientationSensor") {
        updateOrientationStatus(obj);
        emit orientationStatusUpdated();
    }
    
    return true;
}

const TelescopeData& TelescopeDataProcessor::getData() const {
    return telescopeData;
}

void TelescopeDataProcessor::updateMountStatus(const QJsonObject &obj) {
    telescopeData.mount.batteryCurrent = obj["BatteryCurrent"].toDouble();
    telescopeData.mount.batteryLevel = obj["BatteryLevel"].toString();
    telescopeData.mount.batteryVoltage = obj["BatteryVoltage"].toDouble();
    telescopeData.mount.chargerStatus = obj["ChargerStatus"].toString();
    telescopeData.mount.date = obj["Date"].toString();
    telescopeData.mount.time = obj["Time"].toString();
    telescopeData.mount.timeZone = obj["TimeZone"].toString();
    telescopeData.mount.latitude = obj["Latitude"].toDouble();
    telescopeData.mount.longitude = obj["Longitude"].toDouble();
    telescopeData.mount.isAligned = obj["IsAligned"].toBool();
    telescopeData.mount.isGotoOver = obj["IsGotoOver"].toBool();
    telescopeData.mount.isTracking = obj["IsTracking"].toBool();
    telescopeData.mount.numAlignRefs = obj["NumAlignRefs"].toInt();
    telescopeData.mount.altitude = obj["Alt"].toDouble();
    telescopeData.mount.altitudeError = obj["AltitudeError"].toDouble();
    telescopeData.mount.azimuth = obj["Azm"].toDouble();
    telescopeData.mount.azimuthError = obj["AzimuthError"].toDouble();
    
    telescopeData.mount.enc0 = obj["Enc0"].toDouble();
    telescopeData.mount.enc1 = obj["Enc1"].toDouble();
    
    telescopeData.mountLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateCameraStatus(const QJsonObject &obj) {
    telescopeData.camera.binning = obj["Binning"].toInt();
    telescopeData.camera.bitDepth = obj["BitDepth"].toInt();
    telescopeData.camera.colorBBalance = obj["ColorBBalance"].toDouble();
    telescopeData.camera.colorGBalance = obj["ColorGBalance"].toDouble();
    telescopeData.camera.colorRBalance = obj["ColorRBalance"].toDouble();
    telescopeData.camera.exposure = obj["Exposure"].toDouble();
    telescopeData.camera.iso = obj["ISO"].toInt();
    telescopeData.camera.offset = obj["Offset"].toInt();
    
    telescopeData.cameraLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateFocuserStatus(const QJsonObject &obj) {
    telescopeData.focuser.backlash = obj["Backlash"].toInt();
    telescopeData.focuser.calibrationLowerLimit = obj["CalibrationLowerLimit"].toInt();
    telescopeData.focuser.calibrationUpperLimit = obj["CalibrationUpperLimit"].toInt();
    telescopeData.focuser.isCalibrationComplete = obj["IsCalibrationComplete"].toBool();
    telescopeData.focuser.isMoveToOver = obj["IsMoveToOver"].toBool();
    telescopeData.focuser.needAutoFocus = obj["NeedAutoFocus"].toBool();
    telescopeData.focuser.percentageCalibrationComplete = obj["PercentageCalibrationComplete"].toInt();
    telescopeData.focuser.position = obj["Position"].toInt();
    telescopeData.focuser.requiresCalibration = obj["RequiresCalibration"].toBool();
    telescopeData.focuser.velocity = obj["Velocity"].toDouble();
    
    telescopeData.focuserLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateEnvironmentStatus(const QJsonObject &obj) {
    telescopeData.environment.ambientTemperature = obj["AmbientTemperature"].toDouble();
    telescopeData.environment.cameraTemperature = obj["CameraTemperature"].toDouble();
    telescopeData.environment.cpuFanOn = obj["CpuFanOn"].toBool();
    telescopeData.environment.cpuTemperature = obj["CpuTemperature"].toDouble();
    telescopeData.environment.dewPoint = obj["DewPoint"].toDouble();
    telescopeData.environment.frontCellTemperature = obj["FrontCellTemperature"].toDouble();
    telescopeData.environment.humidity = obj["Humidity"].toDouble();
    telescopeData.environment.otaFanOn = obj["OtaFanOn"].toBool();
    telescopeData.environment.recalibrating = obj["Recalibrating"].toBool();
    
    telescopeData.environmentLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateImageInfo(const QJsonObject &obj) {
    telescopeData.lastImage.fileLocation = obj["FileLocation"].toString();
    telescopeData.lastImage.imageType = obj["ImageType"].toString();
    telescopeData.lastImage.dec = obj["Dec"].toDouble();
    telescopeData.lastImage.ra = obj["Ra"].toDouble();
    telescopeData.lastImage.orientation = obj["Orientation"].toDouble();
    telescopeData.lastImage.fovX = obj["FovX"].toDouble();
    telescopeData.lastImage.fovY = obj["FovY"].toDouble();
    
    telescopeData.imageLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateDiskStatus(const QJsonObject &obj) {
    telescopeData.disk.capacity = obj["Capacity"].toVariant().toLongLong();
    telescopeData.disk.freeBytes = obj["FreeBytes"].toVariant().toLongLong();
    telescopeData.disk.level = obj["Level"].toString();
    
    telescopeData.diskLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateDewHeaterStatus(const QJsonObject &obj) {
    telescopeData.dewHeater.aggression = obj["Aggression"].toInt();
    telescopeData.dewHeater.heaterLevel = obj["HeaterLevel"].toDouble();
    telescopeData.dewHeater.manualPowerLevel = obj["ManualPowerLevel"].toDouble();
    telescopeData.dewHeater.mode = obj["Mode"].toString();
    
    telescopeData.dewHeaterLastUpdate = QDateTime::currentDateTime();
}

void TelescopeDataProcessor::updateOrientationStatus(const QJsonObject &obj) {
    telescopeData.orientation.altitude = obj["Altitude"].toInt();
    
    telescopeData.orientationLastUpdate = QDateTime::currentDateTime();
}
