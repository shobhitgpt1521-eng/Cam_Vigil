#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QTimer>
#include <QSocketNotifier>
#include <vector>
#include <string>
#include "archiveworker.h"
#include "camerastreams.h" // for CamHWProfile
#include <libudev.h>


class DbWriter;
class ArchiveManager : public QObject {
    Q_OBJECT
public:
    explicit ArchiveManager(QObject* parent = nullptr);
    ~ArchiveManager();

    // Start recording for each camera profile.
    void startRecording(const std::vector<CamHWProfile>& cameraProfiles);
    // Stop all recording threads.
    void stopRecording();
    // Update the segment duration (in seconds) for future recordings.
    void updateSegmentDuration(int seconds);

    // Return the current archive directory.
    QString getArchiveDir() const { return archiveDir; }
    // Check and return the external storage path.
    QString findExternalStoragePath();

public slots:
    void cleanupArchive();

signals:
    // Emitted when a segment is finalized.
    void segmentWritten();

private slots:
    void handleUdevEvent();
    void onUsbMounted(const QString &device, const QString &path);
    void onUsbUnmounted(const QString &device, const QString &path);

private:
    QTimer cleanupTimer;
    std::vector<ArchiveWorker*> workers;
    QString archiveDir;
    int defaultDuration;  // in seconds

    // Udev integration members.
    struct udev* m_udev;
    struct udev_monitor* m_udevMonitor;
    QSocketNotifier* m_socketNotifier;

    // Cache camera profiles for restarting recording when storage is inserted.
    std::vector<CamHWProfile> cameraProfiles;

    void setupUdevMonitor();
    QThread* dbThread = nullptr;
    DbWriter* db = nullptr;
    QString sessionId;
};

#endif // ARCHIVEMANAGER_H
