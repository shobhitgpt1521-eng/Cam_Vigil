#include "archivewidget.h"
#include "videoplayerwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QRegularExpression>
#include <QScrollBar>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDateTime>
#include <opencv2/opencv.hpp>
#include <QTime>
#include <QtConcurrent>
#include <QStandardPaths>

ArchiveWidget::ArchiveWidget(CameraManager* camManager, ArchiveManager* archiveManager, QWidget *parent)
    : QWidget(parent),
      cameraManager(camManager),
      archiveManager(archiveManager)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // -------------------------------
    // Top bar with Archive title and refresh button
    // -------------------------------
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    QLabel *archivesLabel = new QLabel("Archives", this);
    archivesLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    topBarLayout->addWidget(archivesLabel, 0, Qt::AlignLeft);
    topBarLayout->addStretch();

    refreshButton = new QPushButton("Refresh Archives", this);
    buttonSpinner = new QMovie(":/new/spinner/spinner.gif");
    buttonSpinner->setScaledSize(QSize(32, 32));
    refreshButton->setIconSize(QSize(32, 32));
    refreshButton->setMinimumWidth(160);  // Adjust as needed

    refreshButton->setStyleSheet(refreshButton->styleSheet() +
        " QPushButton {"
        "   qproperty-iconAlignment: AlignCenter;"
        "   text-align: center;"
        "}"
    );

    connect(buttonSpinner, &QMovie::frameChanged, this, [this]() {
        refreshButton->setIcon(QIcon(buttonSpinner->currentPixmap()));
    });

    refreshButton->setStyleSheet(
        "QPushButton {"
        "   color: white;"
        "   background-color: #4d4d4d;"
        "   border-radius: 5px;"
        "   padding: 6px 13px;"
        "   font-weight: 900;"
        "   font-size: 18px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #444444;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #222222;"
        "}"
    );
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &ArchiveWidget::refreshBackupDates);
    topBarLayout->addWidget(refreshButton, 0, Qt::AlignRight);

    mainLayout->addLayout(topBarLayout, 0);

    // -------------------------------
    // Content layout: video list and thumbnail preview side-by-side
    // -------------------------------
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // 1) VIDEO LIST
    videoListWidget = new QListWidget(this);
    videoListWidget->setStyleSheet(
        "background-color: #111; "
        "color: white; "
        "font-size: 18px; "
        "font-weight: bold; "
        "border: 1px solid white;"
        "selection-background-color: white; "
        "selection-color: #000000;"
    );
    videoListWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoListWidget->setMinimumSize(300, 200);

    QScrollBar *vScrollBar = videoListWidget->verticalScrollBar();
    vScrollBar->setStyleSheet(
        "QScrollBar:vertical { border: none; background: #222; width: 10px; }"
        "QScrollBar::handle:vertical { background: white; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #00BFFF; }"
        "QScrollBar::sub-line, QScrollBar::add-line { height: 0px; }"
    );

    connect(videoListWidget, &QListWidget::itemClicked,
            this, &ArchiveWidget::showThumbnail);
    connect(videoListWidget, &QListWidget::itemDoubleClicked,
            this, &ArchiveWidget::openVideoPlayer);

    contentLayout->addWidget(videoListWidget);

    // 2) THUMBNAIL FRAME + LABEL
    QFrame *thumbnailFrame = new QFrame(this);
    thumbnailFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    thumbnailFrame->setStyleSheet("border: none; background-color: transparent;");

    QVBoxLayout *thumbnailLayout = new QVBoxLayout(thumbnailFrame);

    thumbnailLabel = new QLabel("No Preview Available", this);
    thumbnailLabel->setScaledContents(true);
    thumbnailLabel->setAlignment(Qt::AlignCenter);
    thumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    thumbnailLabel->setMinimumSize(300, 200);
    thumbnailLabel->setStyleSheet(
        "background-color: #111; "
        "color: white; "
        "font-size: 18px; "
        "font-weight: bold; "
        "border: 1px solid white;"
    );

    thumbnailLayout->addWidget(thumbnailLabel, 0, Qt::AlignCenter);
    contentLayout->addWidget(thumbnailFrame);
    contentLayout->setStretch(0, 1);
    contentLayout->setStretch(1, 1);

    mainLayout->addLayout(contentLayout);

    videoDetailsLabel = new QLabel("", this);
    videoDetailsLabel->setAlignment(Qt::AlignCenter);
    videoDetailsLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    mainLayout->addWidget(videoDetailsLabel, 0, Qt::AlignCenter);

    setLayout(mainLayout);

    //  the archive directory provided by ArchiveManager.
    archiveDir = archiveManager->getArchiveDir();
    refreshBackupDates();
}

//  implementation for loadVideoFiles slot (even if not fully used)
void ArchiveWidget::loadVideoFiles(const QDate &date) {
    qDebug() << "loadVideoFiles() called for date:" << date.toString();
    //  refresh the backup dates.
    refreshBackupDates();
}

void ArchiveWidget::refreshBackupDates() {
    videoListWidget->clear();  // Clear existing list
    refreshButton->setEnabled(false);
    refreshButton->setText("");  // Hide label text
    buttonSpinner->start();
    refreshButton->setIcon(QIcon(buttonSpinner->currentPixmap()));

    connect(buttonSpinner, &QMovie::frameChanged, this, [=]() {
        refreshButton->setIcon(QIcon(buttonSpinner->currentPixmap()));
    });

    archiveDir = archiveManager->getArchiveDir();  // ArchiveManager's path
    if (archiveDir.isEmpty()) {
        qDebug() << "No external storage archive directory available.";
        thumbnailLabel->setText("No external device connected.");
        buttonSpinner->stop();
            refreshButton->setIcon(QIcon());
            refreshButton->setText("Refresh Archives");
            refreshButton->setEnabled(true);
            return;
    }


    QFutureWatcher<QList<VideoMetadata>> *watcher = new QFutureWatcher<QList<VideoMetadata>>(this);

    connect(watcher, &QFutureWatcher<QList<VideoMetadata>>::finished, this, [this, watcher]() {
        const QList<VideoMetadata> metadataList = watcher->result();
        for (const auto &meta : metadataList) {
            QListWidgetItem *item = new QListWidgetItem(meta.displayText);
            item->setData(Qt::UserRole, meta.filePath);
            videoListWidget->addItem(item);
        }

        // Stops the spinner and restore the button text/icon
        buttonSpinner->stop();
        refreshButton->setIcon(QIcon());
        refreshButton->setText("Refresh Archives");
        refreshButton->setEnabled(true);

        videoListWidget->show();
        watcher->deleteLater();
    });


    QFuture<QList<VideoMetadata>> future = QtConcurrent::run(this, &ArchiveWidget::extractVideoMetadata, archiveDir);
    watcher->setFuture(future);
}


QString ArchiveWidget::formatFileName(const QString &rawFileName,
                                      const QString &absolutePath)
{
    QRegularExpression re(R"(archive_cam(\d+)_(\d{8})_(\d{6})\.mkv)");
    QRegularExpressionMatch match = re.match(rawFileName);
    if (!match.hasMatch()) {
        return rawFileName;
    }

    int camIndex = match.captured(1).toInt();
    QString dateStr = match.captured(2);
    QString timeStr = match.captured(3);

    QDate date = QDate::fromString(dateStr, "yyyyMMdd");
    QString formattedDate = date.toString("MMM d, yyyy");

    QTime time = QTime::fromString(timeStr, "hhmmss");
    QString formattedTime = time.toString("HH:mm");

    QString durationStr = getVideoDuration(absolutePath);

    std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
    QString cameraName;
    if (camIndex >= 0 && camIndex < static_cast<int>(profiles.size())) {
        cameraName = QString::fromStdString(profiles[camIndex].displayName);
    } else {
        cameraName = "UnknownCam";
    }

    return QString("%1 | %2 | %3 | %4")
            .arg(cameraName)
            .arg(formattedDate)
            .arg(formattedTime)
            .arg(durationStr);
}

QString ArchiveWidget::formatFileName(const QString &rawFileName, double durationSeconds) {
    QRegularExpression re(R"(archive_cam(\d+)_(\d{8})_(\d{6})\.mkv)");
    QRegularExpressionMatch match = re.match(rawFileName);
    if (!match.hasMatch()) {
        return rawFileName;
    }

    int camIndex = match.captured(1).toInt();
    QString dateStr = match.captured(2);
    QString timeStr = match.captured(3);

    QDate date = QDate::fromString(dateStr, "yyyyMMdd");
    QString formattedDate = date.toString("MMM d, yyyy");

    QTime time = QTime::fromString(timeStr, "hhmmss");
    QString formattedTime = time.toString("HH:mm");

    QTime durationTime(0, 0);
    durationTime = durationTime.addSecs(static_cast<int>(durationSeconds));

    QString durationStr = (durationSeconds < 3600)
        ? durationTime.toString("mm:ss")
        : durationTime.toString("hh:mm:ss");

    std::vector<CamHWProfile> profiles = cameraManager->getCameraProfiles();
    QString cameraName;
    if (camIndex >= 0 && camIndex < static_cast<int>(profiles.size())) {
        cameraName = QString::fromStdString(profiles[camIndex].displayName);
    } else {
        cameraName = "UnknownCam";
    }

    return QString("%1 | %2 | %3 | %4")
            .arg(cameraName)
            .arg(formattedDate)
            .arg(formattedTime)
            .arg(durationStr);
}


QString ArchiveWidget::getVideoDuration(const QString &videoPath) {
    cv::VideoCapture cap(videoPath.toStdString());
    if (!cap.isOpened()) {
        return "00:00";
    }
    double fps = cap.get(cv::CAP_PROP_FPS);
    double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    cap.release();
    if (fps <= 0.0) {
        return "00:00";
    }
    int totalSeconds = static_cast<int>(frameCount / fps);
    QTime time = QTime(0, 0).addSecs(totalSeconds);

    if (totalSeconds < 3600)
        return time.toString("mm:ss");
    else
        return time.toString("hh:mm:ss");
}
double ArchiveWidget::getVideoDurationSeconds(const QString &videoPath) {
    cv::VideoCapture cap(videoPath.toStdString());
    if (!cap.isOpened()) return 0.0;

    double fps = cap.get(cv::CAP_PROP_FPS);
    double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    cap.release();

    if (fps <= 0.0) return 0.0;
    return frameCount / fps;
}

void ArchiveWidget::showThumbnail(QListWidgetItem *item) {
    selectedVideoPath = item->data(Qt::UserRole).toString();
    if (selectedVideoPath.isEmpty()) {
        qDebug() << "No video selected!";
        thumbnailLabel->clear();
        return;
    }
    qDebug() << "Updating preview for:" << selectedVideoPath;
    thumbnailLabel->clear();
    generateThumbnail(selectedVideoPath);
    videoDetailsLabel->setText(item->text());
}

void ArchiveWidget::generateThumbnail(const QString &videoPath) {
    cv::VideoCapture cap(videoPath.toStdString());
    if (!cap.isOpened()) {
        thumbnailLabel->setText("Failed to open video.");
        return;
    }

    cv::Mat frame;
    cap.read(frame);

    if (!frame.empty()) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        thumbnailLabel->setPixmap(QPixmap::fromImage(img));
    } else {
        thumbnailLabel->setText("Failed to load preview.");
    }
    cap.release();
}

void ArchiveWidget::openVideoPlayer(QListWidgetItem *item) {
    QString videoPath = item->data(Qt::UserRole).toString();
    if (videoPath.isEmpty()) {
        qDebug() << "No video file path stored!";
        return;
    }
    qDebug() << "Opening Video Player for:" << videoPath;
    VideoPlayerWindow *playerWindow = new VideoPlayerWindow(videoPath);
    playerWindow->show();
}
QList<VideoMetadata> ArchiveWidget::extractVideoMetadata(const QString& archiveDirPath) {
    QList<VideoMetadata> list;
    QDir dir(archiveDirPath);
    QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.mkv", QDir::Files, QDir::Time);

    QRegularExpression regex("archive_cam(\\d+)_(\\d{8})_(\\d{6})\\.mkv");

    for (const QFileInfo &fileInfo : fileList) {
        const QString fileName = fileInfo.fileName();
        QRegularExpressionMatch match = regex.match(fileName);
        if (!match.hasMatch()) continue;

       // int camIndex = match.captured(1).toInt();
        QString dateStr = match.captured(2);
        QString timeStr = match.captured(3);

        QDate date = QDate::fromString(dateStr, "yyyyMMdd");
        QTime time = QTime::fromString(timeStr, "hhmmss");
        QDateTime timestamp(date, time);

        double duration = getVideoDurationSeconds(fileInfo.absoluteFilePath());

        QString displayName = formatFileName(fileName, duration);  // Optional: refine if needed

        list.append({fileInfo.absoluteFilePath(), displayName, timestamp, duration});
    }

    return list;
}
