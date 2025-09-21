#pragma once
#include <QWidget>
#include <QMap>
#include "db_reader.h"
#include "playback_controls.h"
#include "playback_timeline_controller.h"
#include "playback_segment_index.h"
#include "playback_trim_panel.h"
class PlaybackTimelineView;
class PlaybackTimelineModel;
class QCloseEvent;
class PlaybackVideoBox;
class PlaybackTitleBar;
class PlaybackSideControls;
class PlaybackVideoPlayerGst;
class QThread;
class PlaybackStitchingPlayer;
class PlaybackExporter;
class PlaybackWindow : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackWindow(QWidget* parent=nullptr);
    ~PlaybackWindow();

    // Call this with ".../CamVigilArchives/camvigil.sqlite"
    void openDb(const QString& dbPath);
    void setCameraList(const QStringList& names);
protected:
    void closeEvent(QCloseEvent* e) override;
private:
    PlaybackTitleBar*       titleBar{nullptr};
    PlaybackControlsWidget* controls{};      // hosted inside titleBar
    PlaybackVideoBox*       videoBox{nullptr};
    PlaybackSideControls*   sideControls{nullptr};
    //DB backend (read-only) in its own thread
    DbReader* db{nullptr};
    QVector<int> camIds;           // index-aligned with names we show
    QMap<QString,int> nameToId;    // name → camera_id (for quick lookup)
    static QString toYmd(const QDate& d) { return d.toString("yyyy-MM-dd"); }
    int selectedCamId = -1;
    // Timeline
    PlaybackTimelineView* timelineView{nullptr};
    PlaybackTimelineController* timelineCtl{nullptr};
    qint64 dayStartNs(const QDate&) const;
    qint64 dayEndNs(const QDate&) const;
    QString fmtRangeLocal(qint64 ns0, qint64 ns1) const;
    static QString tid();
    // Video player on its own thread
    void stopPlayer_();
    void stopStitch_();
    PlaybackVideoPlayerGst* player_{nullptr};
    QThread*                playerThread_{nullptr};
    void initPlayer_();
    // Stitching engine on its own thread
    PlaybackStitchingPlayer* stitch_{nullptr};
    QThread*                 stitchThread_{nullptr};
    void initStitch_();

    // Current day window
    qint64 dayStartNs_{0}, dayEndNs_{0};
    PlaybackSegmentIndex segIndex_;
    QDate currentDay_;
    QString lastCamName_;   // remember last selected camera text
    void runGoFor(const QString& camName, const QDate& day);

    // --- Trim/Export UI state ---
    struct TrimRange { bool enabled=false; qint64 start_ns=0; qint64 end_ns=0; };
    TrimRange trim_;
    PlaybackTrimPanel* trimPanel=nullptr;
    void updateTrimClamps_();                    // stop playback at end when in trim mode
    void applyTextEditsToSelection_(qint64 s, qint64 e);

    void startExport_();
        // Export infra
    QThread* exportThread_{nullptr};
    PlaybackExporter* exporter_{nullptr};

private slots:
    void onCamerasReady(const CamList& cams);
    void onDaysReady(int cameraId, const QStringList& ymdList);
    void onSegmentsReady(int cameraId, const SegmentList& segs);
    void onUiCameraChanged(const QString& camName);
    void onUiDateChanged(const QDate& date);
};
