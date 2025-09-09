#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cameramanager.h"
#include "archivemanager.h"
#include "clickablelabel.h"
#include "fullscreenviewer.h"
#include <QPainter>  // Added for watermark drawing
#include <QResizeEvent>
#include "glcontainerwidget.h"
#include <QTimer>
#include "hik_time.h"
#include "playbackwindow.h"


// Helper function to add watermark to a pixmap.
QPixmap addWatermark(const QPixmap& original, const QString& watermarkText) {
    QPixmap pixmap = original.copy();
    QPainter painter(&pixmap);
    // Semi-transparent white text
    painter.setPen(QColor(255, 255, 255, 150));
    painter.setFont(QFont("Arial", 14, QFont::Bold));

    // Determines text position (bottom-left with 10px padding)
    QFontMetrics fm(painter.font());
    int x = 10;  // Padding from left
    int y = pixmap.height() - 10; // Padding from bottom (y is baseline)
    painter.drawText(x, y, watermarkText);
    painter.end();
    return pixmap;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , gridLayout(new QGridLayout)
    , layoutManager(new LayoutManager(gridLayout))
    , streamManager(new StreamManager(this))
    , archiveManager(nullptr)
    , settingsWindow(nullptr)
    , fullScreenViewer(new FullScreenViewer)
    , currentFullScreenIndex(-1)
{
    ui->setupUi(this);

    topNavbar = new Navbar(this);
    toolbar = new Toolbar(this);

    connect(toolbar, &Toolbar::settingsButtonClicked, this, &MainWindow::openSettingsWindow);
    connect(toolbar, &Toolbar::playbackButtonClicked, this, &MainWindow::openPlaybackWindow);


    // Creating CameraManager instance.
    cameraManager = new CameraManager();
    std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
    hik::syncAllAsync(profiles);
    // hourly resync
    timeSyncTimer = new QTimer(this);
    timeSyncTimer->setTimerType(Qt::VeryCoarseTimer);          // low wakeups
    timeSyncTimer->setInterval(60 * 60 * 1000);                // 1 hour
    connect(timeSyncTimer, &QTimer::timeout, this, [this]() {
        hik::syncAllAsync(cameraManager->getCameraProfiles()); // fresh list each tick
    });
    timeSyncTimer->start();
    int numCameras = profiles.size();
    layoutManager->calculateGridDimensions(numCameras, gridRows, gridCols);
    layoutManager->setupLayout(numCameras);

    // clickable labels for each camera feed.
    for (int i = 0; i < numCameras; ++i) {
        ClickableLabel* label = new ClickableLabel(i, this);

        label->setAlignment(Qt::AlignCenter);
        label->setScaledContents(true);
        label->setStyleSheet("border:2px solid #333; border-radius:5px; margin:5px; padding:5px; background:#000;");
        label->showLoading();
        labels.push_back(label);

        int row = i / gridCols;
        int col = i % gridCols;
        gridLayout->addWidget(label, row, col);

        // Connecting each label's clicked signal.
        connect(label, &ClickableLabel::clicked, this, &MainWindow::showFullScreenFeed);
    }

    GLContainerWidget* gridWidget = new GLContainerWidget(this);
    gridWidget->setLayout(gridLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(topNavbar, 0, Qt::AlignTop);
    mainLayout->addWidget(gridWidget, 1);
    mainLayout->addWidget(toolbar, 0, Qt::AlignBottom);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet("background-color: #121212;");
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

    // Starting streaming: now directly use the profiles (which include the suburl for streaming)
    std::vector<QLabel*> labelPtrs(labels.begin(), labels.end());

//    streamManager->startStreaming(profiles, labelPtrs);

    archiveManager = new ArchiveManager(this);
    archiveManager->startRecording(profiles);

    // Forward frame updates from StreamManager, applying watermark with the camera name.


 /*   connect(streamManager, &StreamManager::frameReady, this, [this](int idx, const QPixmap &pixmap){
        if (idx >= 0 && idx < static_cast<int>(labels.size())) {
            // Fetch the current camera profiles (to include any name changes)
            std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
            QString cameraName = QString::fromStdString(profiles[idx].displayName);
            QPixmap watermarkedPixmap = addWatermark(pixmap, cameraName);
            labels[idx]->setPixmap(watermarkedPixmap);

            // Update fullscreen viewer only if it is showing the same feed.
            if (fullScreenViewer->isVisible() && idx == currentFullScreenIndex) {
                fullScreenViewer->setImage(watermarkedPixmap);
            }
        }
    });*/
    QTimer::singleShot(0, this, &MainWindow::startStreamingAsync);
}


void MainWindow::openSettingsWindow() {
    if (!settingsWindow) {
        settingsWindow = new SettingsWindow(archiveManager, cameraManager, this);
    }
    settingsWindow->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    settingsWindow->showFullScreen();
    settingsWindow->raise();
    settingsWindow->activateWindow();
}
void MainWindow::openPlaybackWindow() {
    const QString dbPath =
        (archiveManager ? archiveManager->archiveRoot() + "/camvigil.sqlite" : QString());

    bool created = false;
    if (!playbackWindow) {
        // New window (previous one was never created or was deleted on close)
        playbackWindow = new PlaybackWindow(nullptr);              // keep it top-level
        playbackWindow->setAttribute(Qt::WA_DeleteOnClose, true);  // self-destruct on close
        playbackWindow->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

        connect(playbackWindow, &QObject::destroyed, this, [this]{
            qInfo() << "[Main] PlaybackWindow destroyed, clearing pointer";
            playbackWindow = nullptr; // QPointer auto-nulls, this is just explicit
        });
        created = true;
    }

    // Only (re)open the DB when we just created the window (or when you detect a path change).
    if (created && !dbPath.isEmpty() && QFileInfo::exists(dbPath)) {
        playbackWindow->openDb(dbPath);
    } else if (created) {
        // Fallback: show camera names even if DB doesn't exist yet
        QStringList camNames;
        const auto profiles = cameraManager->getCameraProfiles();
        camNames.reserve(static_cast<int>(profiles.size()));
        for (const auto& p : profiles) {
            camNames << (p.displayName.empty()
                         ? QString::fromStdString(p.url)
                         : QString::fromStdString(p.displayName));
        }
        playbackWindow->setCameraList(camNames);
    }

    // Show/raise the existing (or newly created) window.
    playbackWindow->showFullScreen();
    playbackWindow->raise();
    playbackWindow->activateWindow();
}




void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    for (ClickableLabel* label : labels) {
        label->setMinimumSize(1, 1);
    }
}

void MainWindow::showFullScreenFeed(int index) {
    currentFullScreenIndex = index;
    QVariant pixmapVar = labels[index]->property("pixmap");
    QPixmap pixmap = pixmapVar.value<QPixmap>();
    if (!pixmap.isNull()) {
        fullScreenViewer->setImage(pixmap);
        fullScreenViewer->showFullScreen();
        fullScreenViewer->raise();
    }
}

MainWindow::~MainWindow() {
    streamManager->stopStreaming();
    if (archiveManager) {
        archiveManager->stopRecording();
        delete archiveManager;
    }
    delete layoutManager;
    delete cameraManager;  // Clean up the persistent CameraManager
    delete ui;
}

void MainWindow::startStreamingAsync() {
    QThread* thread = new QThread;
    streamManager = new StreamManager;

    streamManager->moveToThread(thread);

    auto profiles = cameraManager->getCameraProfiles();
    std::vector<QLabel*> labelPtrs(labels.begin(), labels.end());

    connect(thread, &QThread::started, [=]() {
        streamManager->startStreaming(profiles, labelPtrs);
    });

    // Forward frame updates to UI
    connect(streamManager, &StreamManager::frameReady, this, [this](int idx, const QPixmap &pixmap){
        if (idx >= 0 && idx < static_cast<int>(labels.size())) {
            std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
            QString cameraName = QString::fromStdString(profiles[idx].displayName);
            QPixmap watermarkedPixmap = addWatermark(pixmap, cameraName);
            labels[idx]->setPixmap(watermarkedPixmap);

            if (fullScreenViewer->isVisible() && idx == currentFullScreenIndex) {
                fullScreenViewer->setImage(watermarkedPixmap);
            }
        }
    });

   // connect(streamManager, &StreamManager::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, streamManager, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

/*void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        for (auto label : labels) {
                    label->repaint();
                }
                QApplication::processEvents();
        startStreamingAsync();
    }
}*/
