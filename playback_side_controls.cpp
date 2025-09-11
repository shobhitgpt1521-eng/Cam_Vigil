#include "playback_side_controls.h"
#include <QVBoxLayout>
#include <QPushButton>

PlaybackSideControls::PlaybackSideControls(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(10);

    const char* btnStyle =
        "QPushButton{color:#fff;background:#2a2a2a;padding:10px 14px;border:1px solid #444;"
        "border-radius:8px;}"
        "QPushButton:hover{background:#333;border-color:#666;}"
        "QPushButton:pressed{background:#1f1f1f;border-color:#888; padding-top:11px; padding-bottom:9px;}"
        "QPushButton:disabled{color:#888;background:#1a1a1a;border-color:#333;}";

    play_  = new QPushButton("Play",  this);
    pause_ = new QPushButton("Pause", this);
    play_->setStyleSheet(btnStyle);
    pause_->setStyleSheet(btnStyle);
    play_->setFixedHeight(36);
    play_->setMinimumWidth(100);
    pause_->setFixedHeight(36);
    pause_->setMinimumWidth(100);
    play_->setEnabled(false);
    pause_->setEnabled(false);

   lay->addStretch(1);
   lay->addWidget(play_, 0, Qt::AlignHCenter);
    lay->addWidget(pause_, 0, Qt::AlignHCenter);
    lay->addStretch(1);

    connect(play_,  &QPushButton::clicked, this, &PlaybackSideControls::playClicked);
    connect(pause_, &QPushButton::clicked, this, &PlaybackSideControls::pauseClicked);
}

void PlaybackSideControls::setEnabledControls(bool on) {
    play_->setEnabled(on);
    pause_->setEnabled(on);
}
