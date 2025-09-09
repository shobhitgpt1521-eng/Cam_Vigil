#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QVBoxLayout>
#include "layoutmanager.h"
#include "streammanager.h"
#include "navbar.h"
#include "toolbar.h"
#include "settingswindow.h"
#include "archivemanager.h"
#include "clickablelabel.h"      // For clickable labels
#include "fullscreenviewer.h"    // For fullscreen display
#include "cameramanager.h"       // Persistent camera management
#include <QPointer>
class PlaybackWindow;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
//    void showEvent(QShowEvent *event) override;

private slots:
    void openSettingsWindow();
    void openPlaybackWindow();
    void showFullScreenFeed(int index);

private:
    Ui::MainWindow *ui;
    QGridLayout* gridLayout;
    LayoutManager* layoutManager;
    StreamManager* streamManager;
    ArchiveManager* archiveManager;
    std::vector<ClickableLabel*> labels;
    int gridRows;
    int gridCols;
    CameraManager* cameraManager;  // Persistent CameraManager pointer

    Navbar* topNavbar;
    Toolbar* toolbar;
    SettingsWindow* settingsWindow;
    FullScreenViewer* fullScreenViewer; // Reusable fullscreen viewer
    int currentFullScreenIndex;
    QVector<ClickableLabel*> streamDisplayLabels;
        void startStreamingAsync();
        bool streamsStarted = false;

    QTimer* timeSyncTimer = nullptr;   //Manual camera time sync timer to send http request hourly basis
    QPointer<PlaybackWindow> playbackWindow;
};

#endif // MAINWINDOW_H
