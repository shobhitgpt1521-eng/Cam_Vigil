#include "playbackwindow.h"
#include <QDebug>
#include <QMetaType>
#include <QDateTime>
#include <QToolButton>
#include <QCloseEvent>
#include "playback_timeline_view.h"
#include "playback_db_service.h"
#include <QThread>
PlaybackWindow::PlaybackWindow(QWidget* parent)
    : QWidget(parent)
{
    qInfo() << "[PW] ctor tid=" << tid() << "this=" << this;
    setWindowTitle("Playback");
    setAttribute(Qt::WA_DeleteOnClose, true);
    setStyleSheet("background-color:#111; color:white;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    // Top bar
    auto* top = new QWidget(this);
    top->setStyleSheet("background:#1b1b1b; border-bottom:1px solid #333;");
    auto* topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(12,8,12,8);
    title = new QLabel("Playback", top);
    title->setStyleSheet("font-size:20px; font-weight:800; color:white;");
    controls = new PlaybackControlsWidget(top);
    topLay->addWidget(title, 0, Qt::AlignVCenter|Qt::AlignLeft);
    topLay->addStretch(1);                         // space for center / future items
    topLay->addWidget(controls, 0, Qt::AlignRight);
    // Close button (top-right)
       closeBtn = new QToolButton(top);
       closeBtn->setText(QString::fromUtf8("✕"));
       closeBtn->setToolTip("Close");
       closeBtn->setAutoRaise(true);
       closeBtn->setStyleSheet(
            "QToolButton{color:#bbb;padding:2px 8px;border:1px solid #3a3a3a;"
            "border-radius:6px;background:#232323;}"
            "QToolButton:hover{color:#fff;background:#2b2b2b;}"
            "QToolButton:pressed{background:#1c1c1c;}"
        );
        topLay->addWidget(closeBtn, 0, Qt::AlignRight);
        connect(closeBtn, &QToolButton::clicked, this, [this]{ close(); });
    // Body placeholder
    auto* body = new QWidget(this);
    body->setStyleSheet("background:#111;");
    root->addWidget(top);
    root->addWidget(body, 1);
    // bottom 24h timeline view
    timelineView = new PlaybackTimelineView(this);
    timelineView->setStyleSheet("background:#111;");
    root->addWidget(timelineView); // bottom bar
    setLayout(root);
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
        connect(timelineCtl, &PlaybackTimelineController::built, this,
                    [this](const QDate&, const PlaybackTimelineModel& m){
                        timelineView->setModel(&m);
                        controls->setGoIdle();   // explicitly reset button
                    });
        connect(timelineCtl, &PlaybackTimelineController::log, this,
                [](const QString& s){ qInfo().noquote() << s; });
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
}

void PlaybackWindow::closeEvent(QCloseEvent* e) {
    qInfo() << "[PW] closeEvent tid=" << tid();
    controls->setEnabled(false);
    if (timelineCtl) timelineCtl->detach();
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
            return nameToId.value(name, -1);
        });
        controls->setGoIdle();
    // Fetch cameras
    QMetaObject::invokeMethod(db, "listCameras", Qt::QueuedConnection);
}
void PlaybackWindow::onCamerasReady(const CamList& cams) {
    camIds.clear(); nameToId.clear();
    QStringList names; names.reserve(cams.size());

    for (const auto& p : cams) {
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
    if (db && cid > 0) {
        QMetaObject::invokeMethod(db, "listDays", Qt::QueuedConnection,
                                  Q_ARG(int, cid));
    }
    qInfo() << "[Playback] cameraChanged ->" << camName << "id" << cid;
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
void PlaybackWindow::onSegmentsReady(int, const SegmentList&) { /* unused now */ }


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

