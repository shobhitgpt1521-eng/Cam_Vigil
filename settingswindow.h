// settingswindow.h
// ─────────────────
#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPushButton>
#include "archivemanager.h"
#include "archivewidget.h"
#include "cameramanager.h"
#include "timeeditorwidget.h"

class SettingsWindow : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit SettingsWindow(ArchiveManager* archiveManager,
                            CameraManager* cameraManager,
                            QWidget *parent = nullptr);

protected:
    void initializeGL() override;
    void paintGL() override;

private slots:
    void closeWindow();

private:
    ArchiveManager* archiveManager;
    CameraManager* cameraManager;
    ArchiveWidget* archiveWidget;
    QPushButton* closeIconButton;
    QWidget* navbar;
};

#endif // SETTINGSWINDOW_H
