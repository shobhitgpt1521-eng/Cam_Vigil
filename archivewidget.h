#ifndef ARCHIVEWIDGET_H
#define ARCHIVEWIDGET_H

#include <QWidget>
#include <QDate>
#include <QString>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include "videoplayerwindow.h"
#include "cameramanager.h"
#include "archivemanager.h"
#include <QMovie>
struct VideoMetadata {
    QString filePath;
    QString displayText;
    QDateTime timestamp;
    double duration;
};
class ArchiveWidget : public QWidget {
    Q_OBJECT
public:
    //  accepts both CameraManager and ArchiveManager pointers.
    explicit ArchiveWidget(CameraManager* camManager, ArchiveManager* archiveManager, QWidget *parent = nullptr);
    void refreshBackupDates();

signals:
    void dateSelected(const QDate &date);

private slots:
    void loadVideoFiles(const QDate &date);
    void showThumbnail(QListWidgetItem *item);
    void openVideoPlayer(QListWidgetItem *item);

private:
    QMovie *buttonSpinner;
    QMovie *loadingMovie;
    QString archiveDir;
    QPushButton *refreshButton;
    QListWidget *videoListWidget;
    QLabel *thumbnailLabel;
    QLabel *videoDetailsLabel;
    QString selectedVideoPath;
    double getVideoDurationSeconds(const QString &videoPath);
    QString formatFileName(const QString &rawFileName, const QString &absolutePath);
    QString formatFileName(const QString &rawFileName, double durationSeconds);
    QString getVideoDuration(const QString &videoPath);
    void generateThumbnail(const QString &videoPath);

    CameraManager* cameraManager;
    ArchiveManager* archiveManager;  // New pointer for ArchiveManager
    QList<VideoMetadata> extractVideoMetadata(const QString& archiveDirPath);
};

#endif // ARCHIVEWIDGET_H
