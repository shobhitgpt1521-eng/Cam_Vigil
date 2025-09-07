#include "playbackwindow.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

PlaybackWindow::PlaybackWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // central container
    QWidget* root = new QWidget(this);
    root->setStyleSheet("background-color:#121212;");
    QVBoxLayout* v = new QVBoxLayout(root);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(0);

    // top bar / navbar
    QWidget* bar = new QWidget(root);
    bar->setStyleSheet("background-color:#1e1e1e; border-bottom:1px solid #333;");
    QHBoxLayout* h = new QHBoxLayout(bar);
    h->setContentsMargins(16,10,16,10);

    QLabel* title = new QLabel("Playback", bar);
    title->setStyleSheet("color:white; font-size:20px; font-weight:900;");
    h->addWidget(title, 0, Qt::AlignLeft);

    // spacer for future controls
    QWidget* rightSlot = new QWidget(bar);
    rightSlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(rightSlot, 1);

    v->addWidget(bar);

    // placeholder center content
    QLabel* placeholder = new QLabel("Playback view goes here", root);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color:#bbb; font-size:18px;");
    v->addWidget(placeholder, 1);

    setCentralWidget(root);
}
