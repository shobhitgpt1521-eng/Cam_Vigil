#include "playback_timeline_view.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

PlaybackTimelineView::PlaybackTimelineView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(56);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void PlaybackTimelineView::setModel(const PlaybackTimelineModel* m) {
    model_ = m;
    update();
}

void PlaybackTimelineView::setPlayheadNs(qint64 ns_from_midnight) {
    const qint64 d = dayNs_();
    if (ns_from_midnight < 0) ns_from_midnight = 0;
    if (ns_from_midnight >= d) ns_from_midnight = d - 1;
    playheadNs_ = ns_from_midnight;
    update();
}

QRect PlaybackTimelineView::barRect() const {
    return rect().adjusted(16, 16, -16, -24);
}

void PlaybackTimelineView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = barRect();

    // card + base
    p.fillRect(r.adjusted(-4,-6,4,10), QColor(20,20,20));
    p.setPen(QColor("#2f2f2f")); p.drawRect(r.adjusted(-4,-6,4,10));
    p.fillRect(r, QColor("#242424"));
    p.setPen(QColor("#3a3a3a")); p.drawRect(r.adjusted(0,0,-1,-1));

    // grid
    QFont f = p.font(); f.setPointSizeF(f.pointSizeF()*0.9); p.setFont(f);
    p.setPen(QColor(60,60,60));
    for (int q=0; q<=24*4; ++q) {
        const qreal fx = q/(24.0*4.0);
        const qreal x = r.left() + r.width()*fx;
        p.drawLine(QPointF(x, r.center().y()-6), QPointF(x, r.center().y()+6));
    }
    p.setPen(QColor(110,110,110));
    for (int h=0; h<=24; ++h) {
        const qreal fx = h/24.0;
        const qreal x = r.left() + r.width()*fx;
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        const QString lab = QString("%1").arg(h,2,10,QChar('0'));
        const QRect tr(int(x)-14, r.bottom()+4, 28, 14);
        p.drawText(tr, Qt::AlignHCenter|Qt::AlignTop, lab);
    }

    if (model_) {
        p.setPen(Qt::NoPen);
        for (const auto& s : model_->spans()) {
            const qreal fx1 = model_->fractionFor(s.start_ns);
            const qreal fx2 = model_->fractionFor(s.end_ns);
            const int x1 = int(r.left() + fx1 * r.width());
            const int x2 = int(r.left() + fx2 * r.width());
            QRect seg(x1, r.top()+4, qMax(2, x2-x1), r.height()-8);
            p.fillRect(seg, QColor("#3ddc84"));
            p.setPen(QColor("#2aa864")); p.drawRect(seg.adjusted(0,0,-1,-1));
            p.setPen(Qt::NoPen);
        }

        const qint64 covered = model_->totalCoveredNs();
        const qreal pct = (covered / (qreal)dayNs_()) * 100.0;
        p.setPen(QColor(180,180,180));
        p.drawText(QRect(r.left(), r.top()-14, r.width(), 12),
                   Qt::AlignLeft|Qt::AlignVCenter,
                   QString("Coverage: %1%").arg(QString::number(pct, 'f', 1)));
    }

    // red playhead
    const qreal fx = qBound<qreal>(0.0, playheadNs_ / (qreal)dayNs_(), 1.0);
    const int x = int(r.left() + fx * r.width());
    QPen pen(QColor(220, 50, 47)); pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(QPoint(x, r.top()), QPoint(x, r.bottom()));
}

void PlaybackTimelineView::mouseMoveEvent(QMouseEvent* e) {
    const QRect r = barRect();
    if (!r.contains(e->pos())) { setToolTip(QString()); return; }

    const qreal fx = qBound<qreal>(0.0, (e->pos().x() - r.left()) / qreal(r.width()), 1.0);
    const qint64 t_ns = qint64(fx * dayNs_());

    const int hh = int(fx * 24.0);
    const int mm = int(fmod(fx*24.0, 1.0) * 60.0);
    setToolTip(QString("%1:%2").arg(hh,2,10,QChar('0')).arg(mm,2,10,QChar('0')));

    emit hoverTimeNs(t_ns);

    if (dragging_) {
        setPlayheadNs(t_ns); // visual
        if (!dragTick_.isValid() || dragTick_.elapsed() >= kDragEmitMs) {
            dragTick_.restart();
            emit seekRequested(t_ns); // throttled live-seek
        }
    }
}

void PlaybackTimelineView::mousePressEvent(QMouseEvent* e) {
    if (e->button()!=Qt::LeftButton) return;
    const QRect r = barRect();
    if (!r.contains(e->pos())) return;

    const qreal fx = qBound<qreal>(0.0, (e->pos().x() - r.left()) / qreal(r.width()), 1.0);
    const qint64 t = qint64(fx * dayNs_());
    dragging_ = true;
    dragTick_.restart(); // start throttle window
    setPlayheadNs(t);
    emit seekRequested(t); // immediate jump on press
}

void PlaybackTimelineView::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton) dragging_ = false;
}

void PlaybackTimelineView::leaveEvent(QEvent*) {
    setToolTip(QString());
}
