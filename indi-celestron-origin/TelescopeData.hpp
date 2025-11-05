#pragma once

#include <QString>
#include <QDateTime>

// Define data structures to hold different categories of information
struct MountStatus {
    QString batteryLevel;
    double batteryVoltage = 0.0;
    double batteryCurrent = 0.0;
    QString chargerStatus;
    QString date;
    QString time;
    QString timeZone;
    double latitude = 0.0;
    double longitude = 0.0;
    bool isAligned = false;
    bool isGotoOver = false;
    bool isTracking = false;
    int numAlignRefs = 0;
    double altitude = 0.0;
    double azimuth = 0.0;
    double altitudeError = 0.0;
    double azimuthError = 0.0;
    double enc0 = 0.0;
    double enc1 = 0.0;
};

struct CameraStatus {
    int binning = 1;
    int bitDepth = 0;
    double colorBBalance = 0.0;
    double colorGBalance = 0.0;
    double colorRBalance = 0.0;
    double exposure = 0.0;
    int iso = 0;
    int offset = 0;
};

struct FocuserStatus {
    int backlash = 0;
    int calibrationLowerLimit = 0;
    int calibrationUpperLimit = 0;
    bool isCalibrationComplete = false;
    bool isMoveToOver = false;
    bool needAutoFocus = false;
    int percentageCalibrationComplete = 0;
    int position = 0;
    bool requiresCalibration = false;
    double velocity = 0.0;
};

struct EnvironmentStatus {
    double ambientTemperature = 0.0;
    double cameraTemperature = 0.0;
    bool cpuFanOn = false;
    double cpuTemperature = 0.0;
    double dewPoint = 0.0;
    double frontCellTemperature = 0.0;
    double humidity = 0.0;
    bool otaFanOn = false;
    bool recalibrating = false;
};

struct ImageInfo {
    QString fileLocation;
    QString imageType;
    double dec = 0.0;
    double ra = 0.0;
    double orientation = 0.0;
    double fovX = 0.0;
    double fovY = 0.0;
};

struct DiskStatus {
    qint64 capacity = 0;
    qint64 freeBytes = 0;
    QString level;
};

struct DewHeaterStatus {
    int aggression = 0;
    double heaterLevel = 0.0;
    double manualPowerLevel = 0.0;
    QString mode;
};

struct OrientationStatus {
    int altitude = 0;
};

// Main struct to contain all telescope data
struct TelescopeData {
    MountStatus mount;
    CameraStatus camera;
    FocuserStatus focuser;
    EnvironmentStatus environment;
    ImageInfo lastImage;
    DiskStatus disk;
    DewHeaterStatus dewHeater;
    OrientationStatus orientation;
    
    // Timestamp of last update for each component
    QDateTime mountLastUpdate;
    QDateTime cameraLastUpdate;
    QDateTime focuserLastUpdate;
    QDateTime environmentLastUpdate;
    QDateTime imageLastUpdate;
    QDateTime diskLastUpdate;
    QDateTime dewHeaterLastUpdate;
    QDateTime orientationLastUpdate;
};
