#pragma once
#include <QWidget>
#include "playback_timeline_model.h"

class PlaybackTimelineView : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackTimelineView(QWidget* parent=nullptr);
    void setModel(const PlaybackTimelineModel* m);
signals:
   void hoverTimeNs(qint64 t_ns);
   void seekRequested(qint64 t_ns);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    QSize sizeHint() const override { return {800, 64}; }

private:
    const PlaybackTimelineModel* model_{nullptr};
    QRect barRect() const;          // padded drawing rect
    qint64 t0_{0}, t1_{0};          // cache for tooltips
};
