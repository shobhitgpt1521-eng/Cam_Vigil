

#include "playbackwindow.h"
#include <QDebug>
#include <QMetaType>
#include <QDateTime>
#include <QCloseEvent>
#include "playback_timeline_view.h"
#include "playback_db_service.h"
#include "playback_video_box.h"
#include "playback_title_bar.h"
#include "playback_side_controls.h"
#include <QThread>
#include <QVBoxLayout>
#include "playback_video_player_gst.h"
#include "playback_stitching_player.h"
PlaybackWindow::PlaybackWindow(QWidget* parent)
    : QWidget(parent)
{
    qInfo() << "[PW] ctor tid=" << tid() << "this=" << this;
    // Register types used via queued invokeMethod / signals
    qRegisterMetaType<quintptr>("quintptr");
    qRegisterMetaType<PlaybackVideoPlayerGst*>("PlaybackVideoPlayerGst*");
    qRegisterMetaType<SegmentMeta>("SegmentMeta");
    qRegisterMetaType<QVector<SegmentMeta>>("QVector<SegmentMeta>");
    setWindowTitle("Playback");
    setAttribute(Qt::WA_DeleteOnClose, true);
    setStyleSheet("background-color:#111; color:white;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    // Header bar (title + controls + close)
        titleBar = new PlaybackTitleBar(this);
        titleBar->setTitle("Playback");
        controls = new PlaybackControlsWidget(titleBar);
        titleBar->setRightWidget(controls);
        connect(titleBar, &PlaybackTitleBar::closeRequested, this, [this]{ close(); });
    // Body placeholder
    auto* body = new QWidget(this);
    body->setStyleSheet("background:#111;");
    auto* bodyLay = new QHBoxLayout(body);
        bodyLay->setContentsMargins(12,12,12,12);
        bodyLay->setSpacing(12);

        // --- New: Video box (left) + stacked controls (right) ---
        videoBox = new PlaybackVideoBox(body);
        videoBox->setPlaceholder(QStringLiteral("Please select the camera and date"));
        videoBox->setStyleSheet("background:#111;");
        videoBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

           sideControls = new PlaybackSideControls(body);
           sideControls->setFixedWidth(140);
            // (signals ready for wiring later)
            // connect(sideControls, &PlaybackSideControls::playClicked,  ...);
            // connect(sideControls, &PlaybackSideControls::pauseClicked, ...);

        bodyLay->addWidget(videoBox, 1);
        bodyLay->addWidget(sideControls, 0);

        root->addWidget(titleBar);
        root->addWidget(body, 1);
    // bottom 24h timeline view
    timelineView = new PlaybackTimelineView(this);
    timelineView->setStyleSheet("background:#111;");
    root->addWidget(timelineView); // bottom bar
    setLayout(root);
    initPlayer_();
    initStitch_();
    // Ensure queued connections can deliver these types
    qRegisterMetaType<SegmentInfo>("SegmentInfo");
    qRegisterMetaType<CamList>("CamList");
    qRegisterMetaType<SegmentList>("SegmentList");

    connect(controls, &PlaybackControlsWidget::cameraChanged,
            this, &PlaybackWindow::onUiCameraChanged);
    //connect(controls, &PlaybackControlsWidget::dateChanged,
      //      this, &PlaybackWindow::onUiDateChanged);
    // Controller handles Go→query→build
        timelineCtl = new PlaybackTimelineController(this);
        connect(controls, &PlaybackControlsWidget::goPressed,
                timelineCtl, &PlaybackTimelineController::onGo);
        connect(controls, &PlaybackControlsWidget::goPressed, this,
                    [this](const QString& /*cam*/, const QDate& day){ currentDay_ = day; });
        connect(timelineCtl, &PlaybackTimelineController::built, this,
                    [this](const QDate&, const PlaybackTimelineModel& m){
                        timelineView->setModel(&m);
                        controls->setGoIdle();   // explicitly reset button
                    });
        connect(timelineCtl, &PlaybackTimelineController::log, this,
                [](const QString& s){ qInfo().noquote() << s; });
        // Timeline seek → stitching seek (wall clock)
            connect(timelineView, &PlaybackTimelineView::seekRequested, this, [this](qint64 day_offset_ns){
                if (!stitch_) return;
                const qint64 wall = dayStartNs_ + day_offset_ns;
                QMetaObject::invokeMethod(stitch_, "seekWall", Qt::QueuedConnection,
                                          Q_ARG(qint64, wall));
            });

            // Note: Side controls connections moved to initStitch_() to ensure proper timing
}
void PlaybackWindow::stopPlayer_() {
    if (!playerThread_) return;
    qInfo() << "[PW] stopPlayer_";
    if (player_) {
        // Ensure GStreamer is shut down on its own thread
        QMetaObject::invokeMethod(player_, "teardown",
                                  Qt::BlockingQueuedConnection);
        player_ = nullptr;
    }
    playerThread_->quit();
    if (!playerThread_->wait(3000)) {
        qWarning() << "[PW] player thread didn't quit in time, terminating…";
        playerThread_->terminate();
        playerThread_->wait();
    }
    playerThread_->deleteLater();
    playerThread_ = nullptr;
}

void PlaybackWindow::stopStitch_() {
    if (!stitchThread_) return;
    qInfo() << "[PW] stopStitch_";
    if (stitch_) {
        // provide a no-op if not implemented yet
        QMetaObject::invokeMethod(stitch_, "stop",
                                  Qt::BlockingQueuedConnection);
        stitch_ = nullptr;
    }
    stitchThread_->quit();
    if (!stitchThread_->wait(3000)) {
        qWarning() << "[PW] stitch thread didn't quit in time, terminating…";
        stitchThread_->terminate();
        stitchThread_->wait();
    }
    stitchThread_->deleteLater();
    stitchThread_ = nullptr;
}

void PlaybackWindow::setCameraList(const QStringList& names) {
    controls->setCameraList(names);
}
QString PlaybackWindow::tid() {
    // QThread::currentThreadId() returns a pointer-like handle; stringify as hex
    const quintptr p = reinterpret_cast<quintptr>(QThread::currentThreadId());
    return QString::number(p, 16);
}
PlaybackWindow::~PlaybackWindow() {
    qInfo() << "[PW] dtor tid=" << tid() << "this=" << this;
    // Ensure background threads are down even if window is destroyed externally
    stopStitch_();
    stopPlayer_();
}

void PlaybackWindow::closeEvent(QCloseEvent* e) {
    qInfo() << "[PW] closeEvent tid=" << tid();
    controls->setEnabled(false);
    if (timelineCtl) timelineCtl->detach();
    // Tear down worker threads before letting the widget die
    stopStitch_();
    stopPlayer_();
    e->accept();
}
void PlaybackWindow::openDb(const QString& dbPath) {
    qInfo() << "[PW] openDb(" << dbPath << ") tid=" << tid();
    if (dbPath.isEmpty()) return;
    // Attach to the shared DB service
        auto svc = PlaybackDbService::instance();
        svc->ensureOpened(dbPath);          // idempotent
        DbReader* newDb = svc->reader();

        if (db != newDb) {
           // Rebind signals safely for this window context
            if (db) QObject::disconnect(db, nullptr, this, nullptr);
            db = newDb;
            connect(db, &DbReader::opened, this, [](bool ok, const QString& err){
                if (!ok) qWarning() << "[Playback] DB open failed:" << err;
            });
            connect(db, &DbReader::camerasReady, this,
                    &PlaybackWindow::onCamerasReady, Qt::QueuedConnection);
            connect(db, &DbReader::daysReady,     this,
                    &PlaybackWindow::onDaysReady, Qt::QueuedConnection);
            connect(db, &DbReader::segmentsReady, this,
                    &PlaybackWindow::onSegmentsReady, Qt::QueuedConnection);
            connect(db, &DbReader::error,         this,
                    [](const QString& e){ qWarning() << "[Playback] DB error:" << e; });
        }

        // Attach controller to shared DbReader and set resolver
        timelineCtl->attach(db);
        timelineCtl->setCameraResolver([this](const QString& name){
            int id = nameToId.value(name, -1);
            qInfo() << "[PW] Camera resolver: '" << name << "' -> ID" << id;
            return id;
        });
        controls->setGoIdle();
    // Fetch cameras
    QMetaObject::invokeMethod(db, "listCameras", Qt::QueuedConnection);
}
void PlaybackWindow::onCamerasReady(const CamList& cams) {
    camIds.clear(); nameToId.clear();
    QStringList names; names.reserve(cams.size());

    qInfo() << "[PW] onCamerasReady - received" << cams.size() << "cameras from database:";
    for (const auto& p : cams) {
        qInfo() << "  ID:" << p.first << "Name:" << p.second;
        camIds << p.first;
        nameToId.insert(p.second, p.first);
        names << p.second;
    }
    
    setCameraList(names);  // this calls controls->setCameraList(names)
        if (!names.isEmpty()) {
        controls->setDate(QDate::currentDate());
        //onUiCameraChanged(names.first());
    }
}
void PlaybackWindow::onUiCameraChanged(const QString& camName) {
    const int cid = nameToId.value(camName, -1);
    selectedCamId = cid;
    
    qInfo() << "[Playback] cameraChanged ->" << camName << "id" << cid;
    qInfo() << "[Playback] Available cameras in nameToId:";
    for (auto it = nameToId.begin(); it != nameToId.end(); ++it) {
        qInfo() << "  " << it.key() << "->" << it.value();
    }
    
    if (db && cid > 0) {
        QMetaObject::invokeMethod(db, "listDays", Qt::QueuedConnection,
                                  Q_ARG(int, cid));
    } else {
        qWarning() << "[Playback] Cannot list days: db=" << (db != nullptr) << "cid=" << cid;
        qWarning() << "[Playback] This usually means the camera name doesn't match the database.";
        qWarning() << "[Playback] Check that camera names in cameras.json match those in the database.";
    }
}
void PlaybackWindow::onDaysReady(int cameraId, const QStringList& ymdList) {
    if (cameraId != selectedCamId) return;

    if (ymdList.isEmpty()) {
        controls->setDateBounds(QDate(), QDate());
        qInfo() << "[Playback] No recordings for camera" << cameraId;
        return;
    }

    QDate minD = QDate::fromString(ymdList.first(), "yyyy-MM-dd");
    QDate maxD = minD;
   QSet<QDate> avail;
    for (const auto& ymd : ymdList) {
        const QDate d = QDate::fromString(ymd, "yyyy-MM-dd");
        if (d.isValid()) {
            avail.insert(d);
            if (d < minD) minD = d;
            if (d > maxD) maxD = d;
        }
    }
    controls->setDateBounds(minD, maxD);
    controls->setAvailableDates(avail);
    controls->setDate(maxD);
}
void PlaybackWindow::onUiDateChanged(const QDate&) { /* no-op by design */ }
void PlaybackWindow::onSegmentsReady(int cameraId, const SegmentList& segs) {
    if (cameraId != selectedCamId) return;
    if (!controls) return;
    const QDate day = currentDay_.isValid() ? currentDay_ : QDate::currentDate();
    if (!day.isValid()) return;

    qInfo() << "[PW] segmentsReady count=" << segs.size()
           << "cid=" << cameraId
           << "day=" << (currentDay_.isValid()? currentDay_.toString("yyyy-MM-dd")
                                              : QDate::currentDate().toString("yyyy-MM-dd"));
    qInfo() << "[PW] segmentsReady count=" << segs.size()
                << "cid=" << cameraId
                << "day=" << (currentDay_.isValid()? currentDay_.toString("yyyy-MM-dd")
                                                   : QDate::currentDate().toString("yyyy-MM-dd"));

        if (!segs.isEmpty()) {
            qint64 minStart = segs.first().start_ns, maxStart = segs.first().start_ns;
            qint64 minEnd   = segs.first().end_ns,   maxEnd   = segs.first().end_ns;
            for (const auto& s : segs) {
                minStart = std::min(minStart, s.start_ns);
                maxStart = std::max(maxStart, s.start_ns);
                minEnd   = std::min(minEnd,   s.end_ns);
                maxEnd   = std::max(maxEnd,   s.end_ns);
            }
            qInfo() << "[PW] segs range start_ns=[" << minStart << ".." << maxStart
                    << "] end_ns=[" << minEnd << ".." << maxEnd << "]";
       }

    // Compute day window (local midnight)
    dayStartNs_ = dayStartNs(day);
    dayEndNs_   = dayEndNs(day);

    // Build segment index (detect gaps, normalize)
    segIndex_.build(segs, dayStartNs_, dayEndNs_);
    segIndex_.debugDump("SegIndex");

    // Export to metas for stitching (virtual timeline)
    QVector<QString> paths;
    QVector<qint64>  wallStarts, offsets, durations;
    segIndex_.exportForStitching(paths, wallStarts, offsets, durations);

    QVector<SegmentMeta> metas;
    metas.reserve(paths.size());
    for (int i=0;i<paths.size();++i) {
        metas.push_back({ paths[i],
                          dayStartNs_ + wallStarts[i],
                          offsets[i],
                         durations[i] });
    }
    // Feed stitching engine
    if (stitch_) {
        QMetaObject::invokeMethod(stitch_, "setPlaylist", Qt::QueuedConnection,
                                  Q_ARG(QVector<SegmentMeta>, metas),
                                  Q_ARG(qint64, dayStartNs_));
    }
    qInfo() << "[PW] sideControls=" << sideControls << " enable=" << !metas.isEmpty()
                << " metas=" << metas.size();
        if (sideControls) sideControls->setEnabledControls(!metas.isEmpty());
}

    // helper to compute day window (local midnight)
    static inline qint64 toNs(qint64 s) { return s * 1000000000LL; }
    qint64 PlaybackWindow::dayStartNs(const QDate& d) const {
        const QString s = d.toString("yyyy-MM-dd") + " 00:00:00";
        const auto dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss").toLocalTime();
        return toNs(dt.toSecsSinceEpoch());
   }


    qint64 PlaybackWindow::dayEndNs(const QDate& d) const {
       return dayStartNs(d.addDays(1));
}

    QString PlaybackWindow::fmtRangeLocal(qint64 ns0, qint64 ns1) const {
        const auto s0 = QDateTime::fromSecsSinceEpoch(ns0/1000000000LL).toLocalTime();
        const auto s1 = QDateTime::fromSecsSinceEpoch(ns1/1000000000LL).toLocalTime();
        return QString("%1 → %2  (%3 s)")
              .arg(s0.toString("HH:mm:ss"))
              .arg(s1.toString("HH:mm:ss"))
              .arg((ns1-ns0)/1000000000LL);
}
    void PlaybackWindow::initPlayer_() {
        // Create a dedicated thread for the player backend
        playerThread_ = new QThread(this);
        player_ = new PlaybackVideoPlayerGst();
        player_->moveToThread(playerThread_);
        connect(playerThread_, &QThread::finished, player_, &QObject::deleteLater);
        playerThread_->start();

        // Bind the sink window handle
        const quintptr wid = videoBox->renderWinId();
        QMetaObject::invokeMethod(player_, "setWindowHandle", Qt::QueuedConnection,
                                  Q_ARG(quintptr, wid));
        // Start timers on the player thread
            QMetaObject::invokeMethod(player_, "startTimers", Qt::QueuedConnection);

       // Optional: basic error log
        connect(player_, &PlaybackVideoPlayerGst::errorText, this,
                [](const QString& e){ qWarning() << "[Player]" << e; });
    }
    void PlaybackWindow::initStitch_() {
        stitchThread_ = new QThread(this);
        stitch_ = new PlaybackStitchingPlayer();
        stitch_->moveToThread(stitchThread_);
        connect(stitchThread_, &QThread::finished, stitch_, &QObject::deleteLater);
        stitchThread_->start();

        // Attach the threaded player
        QMetaObject::invokeMethod(stitch_, "attachPlayer", Qt::QueuedConnection,
                                  Q_ARG(PlaybackVideoPlayerGst*, player_));

        // Stitching → playhead
        connect(stitch_, &PlaybackStitchingPlayer::wallPositionNs, this,
                [this](qint64 wall_offset_ns){
                    timelineView->setPlayheadNs(wall_offset_ns);
                }, Qt::QueuedConnection);

        // Side controls → Stitcher (queued across threads, type-safe)
        // Moved here to ensure stitch_ is properly initialized
        connect(sideControls, &PlaybackSideControls::playClicked, this,
                [this](){
                    qInfo() << "[PW] Play button clicked";
                    if (!stitch_) {
                        qWarning() << "[PW] Stitching player not available";
                        return;
                    }
                    QMetaObject::invokeMethod(stitch_, "play", Qt::QueuedConnection);
                });
        connect(sideControls, &PlaybackSideControls::pauseClicked, this,
                [this](){
                    qInfo() << "[PW] Pause button clicked";
                    if (!stitch_) {
                        qWarning() << "[PW] Stitching player not available";
                        return;
                    }
                    QMetaObject::invokeMethod(stitch_, "pause", Qt::QueuedConnection);
                });

        // Optional: see activity in logs
        connect(stitch_, &PlaybackStitchingPlayer::segmentChanged, this,
                [](int i){ qInfo() << "[Stitch] segmentChanged" << i; },
                Qt::QueuedConnection);

        // Handle state changes
        connect(stitch_, &PlaybackStitchingPlayer::stateChanged, this,
                [this](bool playing){
                    qInfo() << "[PW] Playback state changed to:" << (playing ? "PLAYING" : "PAUSED");
                    // You could update UI elements here if needed
                }, Qt::QueuedConnection);

        // Optional: log errors
        connect(stitch_, &PlaybackStitchingPlayer::errorText, this,
                [](const QString& e){ qWarning() << "[Stitch]" << e; }, Qt::QueuedConnection);
    }
