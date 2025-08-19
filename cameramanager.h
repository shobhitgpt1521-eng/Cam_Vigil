#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include "camerastreams.h"
#include <vector>
#include <string>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QJsonArray>

class CameraManager {
public:
    CameraManager();

    std::vector<CamHWProfile> getCameraProfiles();
    std::vector<std::string> getCameraUrls();

    // New functions for persistence
    void renameCamera(int index, const std::string& newName);
    void saveCameraNames();  // Save names to JSON file
    void loadCameraNames();  // Load names from JSON file

private:
    std::string configFilePath;
};

#endif // CAMERAMANAGER_H
