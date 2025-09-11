#include "playback_video_box.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFont>

PlaybackVideoBox::PlaybackVideoBox(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 180);
    setAutoFillBackground(false);
}

void PlaybackVideoBox::setPlaceholder(const QString& text) {
    placeholder_ = text;
    update();
}

void PlaybackVideoBox::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Card-style background
    QRect r = rect().adjusted(12, 12, -12, -12);
    p.fillRect(r, QColor(17,17,17));                 // #111
    p.setPen(QColor("#2f2f2f"));
    p.drawRect(r.adjusted(0,0,-1,-1));

    // Inner border to suggest “video frame”
    QRect inner = r.adjusted(8,8,-8,-8);
    p.setPen(QColor("#3a3a3a"));
    p.drawRect(inner.adjusted(0,0,-1,-1));

    // Placeholder text centered
    QFont f = p.font();
    f.setPointSizeF(f.pointSizeF()*1.1);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(180,180,180));
    p.drawText(inner, Qt::AlignCenter, placeholder_);
}
