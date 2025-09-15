#pragma once
#include <QWidget>
#include <QElapsedTimer>
#include "playback_timeline_model.h"

class PlaybackTimelineView : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackTimelineView(QWidget* parent=nullptr);
    void setModel(const PlaybackTimelineModel* m);
    void setPlayheadNs(qint64 ns_from_midnight);
    qint64 playheadNs() const { return playheadNs_; }

signals:
    void hoverTimeNs(qint64 t_ns);
    void seekRequested(qint64 t_ns);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    QSize sizeHint() const override { return {800, 64}; }

private:
    const PlaybackTimelineModel* model_{nullptr};
    QRect  barRect() const;
    qint64 dayNs_() const { return 24LL*3600LL*1000000000LL; }

    qint64       playheadNs_ = 0;
    bool         dragging_   = false;
    QElapsedTimer dragTick_;
    static constexpr int kDragEmitMs = 90; // ~11 Hz while dragging
};
