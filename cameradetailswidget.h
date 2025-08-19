#ifndef CAMERADETAILSWIDGET_H
#define CAMERADETAILSWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include "cameramanager.h"

class CameraDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraDetailsWidget(CameraManager* cameraManager, QWidget* parent = nullptr);

private slots:
    void onCameraSelectionChanged(int comboIndex);
    void onNameEditingFinished();

private:
    void loadCameraInfo(int cameraIndex);

    CameraManager* cameraManager;
    QComboBox* cameraCombo;      // Dropdown for cameras
    QLineEdit* nameEdit;         // Input for camera name
    int currentCameraIndex;      // Tracks selected camera index
};

#endif // CAMERADETAILSWIDGET_H
