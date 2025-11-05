#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include "TelescopeData.hpp"

/**
 * @brief Processes telescope data from JSON packets
 * 
 * This class is responsible for parsing JSON packets from the telescope
 * and updating the appropriate data structures.
 */
class TelescopeDataProcessor : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Constructor
     * @param parent The parent QObject
     */
    TelescopeDataProcessor(QObject *parent = nullptr);
    
    /**
     * @brief Reset all data to default values
     */
    void reset();
    
    /**
     * @brief Process a JSON packet from the telescope
     * @param jsonData The JSON data to process
     * @return true if the packet was processed successfully, false otherwise
     */
    bool processJsonPacket(const QByteArray &jsonData);
    
    /**
     * @brief Get the current telescope data
     * @return A reference to the telescope data
     */
    const TelescopeData& getData() const;
    
signals:
    /** Signal emitted when mount status is updated */
    void mountStatusUpdated();
    
    /** Signal emitted when camera status is updated */
    void cameraStatusUpdated();
    
    /** Signal emitted when focuser status is updated */
    void focuserStatusUpdated();
    
    /** Signal emitted when environment status is updated */
    void environmentStatusUpdated();
    
    /** Signal emitted when a new image is available */
    void newImageAvailable();
    
    /** Signal emitted when disk status is updated */
    void diskStatusUpdated();
    
    /** Signal emitted when dew heater status is updated */
    void dewHeaterStatusUpdated();
    
    /** Signal emitted when orientation status is updated */
    void orientationStatusUpdated();
    
private:
    /** The telescope data */
    TelescopeData telescopeData;
    
    /**
     * @brief Update mount status from JSON
     * @param obj The JSON object containing mount data
     */
    void updateMountStatus(const QJsonObject &obj);
    
    /**
     * @brief Update camera status from JSON
     * @param obj The JSON object containing camera data
     */
    void updateCameraStatus(const QJsonObject &obj);
    
    /**
     * @brief Update focuser status from JSON
     * @param obj The JSON object containing focuser data
     */
    void updateFocuserStatus(const QJsonObject &obj);
    
    /**
     * @brief Update environment status from JSON
     * @param obj The JSON object containing environment data
     */
    void updateEnvironmentStatus(const QJsonObject &obj);
    
    /**
     * @brief Update image info from JSON
     * @param obj The JSON object containing image data
     */
    void updateImageInfo(const QJsonObject &obj);
    
    /**
     * @brief Update disk status from JSON
     * @param obj The JSON object containing disk data
     */
    void updateDiskStatus(const QJsonObject &obj);
    
    /**
     * @brief Update dew heater status from JSON
     * @param obj The JSON object containing dew heater data
     */
    void updateDewHeaterStatus(const QJsonObject &obj);
    
    /**
     * @brief Update orientation status from JSON
     * @param obj The JSON object containing orientation data
     */
    void updateOrientationStatus(const QJsonObject &obj);
};
