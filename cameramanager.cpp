#include "cameramanager.h"
#include "camerastreams.h"
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonDocument>

CameraManager::CameraManager() {
    // Set the config file path (in the application's current directory)
    configFilePath = QDir::currentPath().toStdString() + "/cameras.json";
    // Populates the CameraStreams from JSON if its vector is empty.
    CameraStreams::loadFromJson();
}

std::vector<CamHWProfile> CameraManager::getCameraProfiles() {
    return CameraStreams::getCameraUrls();
}

std::vector<std::string> CameraManager::getCameraUrls() {
    std::vector<CamHWProfile> profiles = getCameraProfiles();
    std::vector<std::string> urls;
    for (const auto& profile : profiles) {
        urls.push_back(profile.url);
    }
    return urls;
}

void CameraManager::renameCamera(int index, const std::string& newName) {
    // Update the camera's display name and save the configuration.
    CameraStreams::setCameraDisplayName(index, newName);
    saveCameraNames();
}

void CameraManager::saveCameraNames() {
    QJsonObject json;
    QJsonArray camerasArray;

    std::vector<CamHWProfile> profiles = getCameraProfiles();
    for (const auto& profile : profiles) {
        QJsonObject camObj;
        camObj["url"] = QString::fromStdString(profile.url);
        camObj["suburl"] = QString::fromStdString(profile.suburl); // Save the suburl as well.
        camObj["name"] = QString::fromStdString(profile.displayName);
        camerasArray.append(camObj);
    }
    json["cameras"] = camerasArray;

    QFile file(QString::fromStdString(configFilePath));
    // Open file in WriteOnly mode with Truncate to overwrite old data.
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(json).toJson());
        file.close();
    } else {
        qDebug() << "Unable to open file for writing:" << QString::fromStdString(configFilePath);
    }
}
