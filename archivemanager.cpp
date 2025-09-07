#include "archivemanager.h"
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QStorageInfo>
#include <QFileInfo>
#include <QRegExp>
#include <libudev.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QVariant>
#include <QVariantList>
#include <QDBusMetaType>   // For qDBusRegisterMetaType<...>()

#include "db_writer.h"
#include <QUuid>



ArchiveManager::ArchiveManager(QObject *parent)
    : QObject(parent),
      defaultDuration(300),  // 5-minute default
      m_udev(nullptr),
      m_udevMonitor(nullptr),
      m_socketNotifier(nullptr)
{
    m_udev = udev_new();
    if (!m_udev) {
        qWarning() << "[ArchiveManager] Cannot create udev";
    } else {
        m_udevMonitor = udev_monitor_new_from_netlink(m_udev, "udev");
        if (!m_udevMonitor) {
            qWarning() << "[ArchiveManager] Cannot create udev monitor";
            udev_unref(m_udev);
            m_udev = nullptr;
        } else {
            udev_monitor_filter_add_match_subsystem_devtype(m_udevMonitor, "block", nullptr);
            udev_monitor_enable_receiving(m_udevMonitor);
            int fd = udev_monitor_get_fd(m_udevMonitor);
            m_socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(m_socketNotifier, &QSocketNotifier::activated,
                    this, &ArchiveManager::handleUdevEvent);
            qDebug() << "[ArchiveManager] Udev monitor setup complete.";
        }
    }
    connect(&cleanupTimer, &QTimer::timeout, this, &ArchiveManager::cleanupArchive);
    cleanupTimer.start(60 * 60 * 1000);  // every hour
    qDebug() << "[ArchiveManager] Initialized.";

    qDBusRegisterMetaType<QVariantList>();
    qDBusRegisterMetaType<QList<QVariantList>>();

    QDBusInterface automountIface(
        "com.idonial.automount",         // service name
        "/com/idonial/automount",        // object path
        "com.idonial.automount",         // interface name
        QDBusConnection::systemBus(),
        this
    );

    // Connecting to “Mounted(QString, QString)”
    bool okMount = QDBusConnection::systemBus().connect(
        "com.idonial.automount",
        "/com/idonial/automount",
        "com.idonial.automount",
        "Mounted",
        this,
        SLOT(onUsbMounted(QString,QString))
    );
    qDebug() << "[ArchiveManager] automount-core Mounted subscription successful?" << okMount;

    // Connecting to “Unmounted(QString, QString)”
    bool okUnmount = QDBusConnection::systemBus().connect(
        "com.idonial.automount",
        "/com/idonial/automount",
        "com.idonial.automount",
        "Unmounted",
        this,
        SLOT(onUsbUnmounted(QString,QString))
    );
    qDebug() << "[ArchiveManager] automount-core Unmounted subscription successful?" << okUnmount;
}
ArchiveManager::~ArchiveManager()
{
    stopRecording();
    if (m_socketNotifier) {
        m_socketNotifier->setEnabled(false);
        delete m_socketNotifier;
    }
    if (m_udevMonitor) {
        udev_monitor_unref(m_udevMonitor);
    }
    if (m_udev) {
        udev_unref(m_udev);
    }
    // stop DB thread
        if (dbThread) {
            dbThread->quit();
            dbThread->wait();
            dbThread = nullptr;
        }
    qDebug() << "[ArchiveManager] Destroyed.";
}

void ArchiveManager::handleUdevEvent()
{
    m_socketNotifier->setEnabled(false);
    struct udev_device *dev = udev_monitor_receive_device(m_udevMonitor);
    if (dev) {
        const char *action = udev_device_get_action(dev);
        const char *devnode = udev_device_get_devnode(dev);
        const char *subsystem = udev_device_get_subsystem(dev);
        qDebug() << "[ArchiveManager] Udev event:"
                 << (action ? action : "unknown")
                 << (devnode ? devnode : "unknown")
                 << (subsystem ? subsystem : "unknown");

        QString externalPath = findExternalStoragePath();
        if (!externalPath.isEmpty()) {
            if (workers.empty()) {
                qDebug() << "[ArchiveManager] External storage detected, starting recording.";
                archiveDir = externalPath + "/CamVigilArchives";
                QDir dir(archiveDir);
                if (!dir.exists()) {
                    dir.mkpath(".");
                }
                startRecording(cameraProfiles);
            }
        } else {
            if (!workers.empty()) {
                qDebug() << "[ArchiveManager] External storage removed, stopping recording.";
                stopRecording();
            }
        }
        udev_device_unref(dev);
    }
    m_socketNotifier->setEnabled(true);
}

static bool isKernelRemovable(const QStorageInfo &storage)
{
    QString devicePath = QString::fromUtf8(storage.device());
    QString devFileName = QFileInfo(devicePath).fileName();
    devFileName.remove(QRegExp("\\d+$"));
    QString sysRemovablePath = QString("/sys/block/%1/removable").arg(devFileName);
    QFile removableFile(sysRemovablePath);
    if (!removableFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray content = removableFile.readAll().trimmed();
    return (content == "1");
}

QString ArchiveManager::findExternalStoragePath()
{
    QList<QStorageInfo> storages = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : storages) {
        if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
            QString rootPath = storage.rootPath();
            if ((rootPath.startsWith("/media") || rootPath.startsWith("/run/media"))
                && isKernelRemovable(storage))
            {
                return rootPath;
            }
        }
    }
    return "";
}

void ArchiveManager::startRecording(const std::vector<CamHWProfile> &camProfiles)
{
    cameraProfiles = camProfiles;
    QString externalPath = findExternalStoragePath();
    if (externalPath.isEmpty()) {
        qDebug() << "[ArchiveManager] No external storage detected. Recording not started.";
        return;
    }
    archiveDir = externalPath + "/CamVigilArchives";
    QDir dir(archiveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    // ---- DB bring-up ----
        if (!dbThread) {
            dbThread = new QThread(this);
            db = new DbWriter();
            db->moveToThread(dbThread);
            connect(dbThread, &QThread::finished, db, &QObject::deleteLater);
            dbThread->start();
            QMetaObject::invokeMethod(db, "openAt", Qt::BlockingQueuedConnection,
                                      Q_ARG(QString, archiveDir + "/camvigil.sqlite"));
        }
        // ensure cameras
        for (const auto& p : camProfiles) {
            QMetaObject::invokeMethod(db, "ensureCamera", Qt::QueuedConnection,
                Q_ARG(QString, QString::fromStdString(p.url)),
                Q_ARG(QString, QString::fromStdString(p.suburl)),
                Q_ARG(QString, QString::fromStdString(p.displayName)));
        }
        // session
        sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QMetaObject::invokeMethod(db, "beginSession", Qt::QueuedConnection,
            Q_ARG(QString, sessionId), Q_ARG(QString, archiveDir), Q_ARG(int, defaultDuration));
        // ----------------------

    QDateTime masterStart = QDateTime::currentDateTime();
    qDebug() << "[ArchiveManager] Master start time:" << masterStart.toString("yyyyMMdd_HHmmss");
    for (size_t i = 0; i < camProfiles.size(); ++i) {
        const auto &profile = camProfiles[i];
        ArchiveWorker* worker = new ArchiveWorker(
            profile.url,
            static_cast<int>(i),
            archiveDir,
            defaultDuration,
            masterStart
        );
        connect(worker, &ArchiveWorker::recordingError, [](const std::string &err){
            qDebug() << "[ArchiveManager] ArchiveWorker error:" << QString::fromStdString(err);
        });
        // segment → DB
        // ----------------------
        connect(worker, &ArchiveWorker::segmentOpened, this,
                        [this, camProfiles](int camIdx, const QString& path, qint64 startNs){
                            const QString camUrl = QString::fromStdString(camProfiles[camIdx].url);
                            QMetaObject::invokeMethod(db, "addSegmentOpened", Qt::QueuedConnection,
                                Q_ARG(QString, sessionId), Q_ARG(QString, camUrl),
                                Q_ARG(QString, path), Q_ARG(qint64, startNs));
                        });
        // ----------------------
        connect(worker, &ArchiveWorker::segmentClosed, this,
                        [this, camProfiles](int camIdx, const QString& path, qint64 endNs, qint64 durMs){
                            Q_UNUSED(camIdx);
                            QMetaObject::invokeMethod(db, "finalizeSegmentByPath", Qt::QueuedConnection,
                                Q_ARG(QString, path), Q_ARG(qint64, endNs), Q_ARG(qint64, durMs));
                        });
        // ----------------------
        connect(worker, &ArchiveWorker::segmentFinalized, this, &ArchiveManager::segmentWritten);
        workers.push_back(worker);
        worker->start();
        qDebug() << "[ArchiveManager] Started ArchiveWorker for cam" << i;
    }
}

void ArchiveManager::stopRecording()
{
    for (auto worker : workers) {
        worker->stop();
        worker->wait();
        delete worker;
    }
    workers.clear();
    qDebug() << "[ArchiveManager] All ArchiveWorkers stopped.";
}

void ArchiveManager::updateSegmentDuration(int seconds)
{
    qDebug() << "[ArchiveManager] Initiating segment duration update to" << seconds << "seconds.";
    for (auto *worker : workers) {
        QMetaObject::invokeMethod(worker, "updateSegmentDuration", Qt::QueuedConnection,
                                  Q_ARG(int, seconds));
    }
}

void ArchiveManager::cleanupArchive()
{
    if (archiveDir.isEmpty()) {
        qDebug() << "[ArchiveManager] No archive directory set.";
        return;
    }
    QDir dir(archiveDir);
    if (!dir.exists()) {
        qDebug() << "[ArchiveManager] Archive directory does not exist:" << archiveDir;
        return;
    }
    qDebug() << "[ArchiveManager] Cleanup complete.";
}

void ArchiveManager::onUsbMounted(const QString &device, const QString &path)
{
    qDebug() << "[ArchiveManager] USB mounted:" << device << "→" << path;
    // Creates “CamVigilArchives” subfolder under that mount
    archiveDir = path + "/CamVigilArchives";
    QDir dir(archiveDir);
    if (!dir.exists()) dir.mkpath(".");

    // Start recording if not already started
    if (workers.empty()) {
        startRecording(cameraProfiles);
        qDebug() << "[ArchiveManager] Recording started in" << archiveDir;
    }
}

void ArchiveManager::onUsbUnmounted(const QString &device, const QString &path)
{
    qDebug() << "[ArchiveManager] USB unmounted:" << device << "→" << path;
    if (!workers.empty()) {
        stopRecording();
        qDebug() << "[ArchiveManager] Recording stopped.";
    }
}
