#include "toolbar.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include "subscriptionmanager.h"
#include <QGridLayout>

Toolbar::Toolbar(QWidget* parent)
    : QWidget(parent),
      networkManager(new QNetworkAccessManager(this)),
      checkTimer(new QTimer(this))
{
    setStyleSheet(
        "color: white;"
        "font-size: 24px;"
        "font-weight: bold;"
        "padding: 10px;"
    );

    // 3-column grid: [left/status] [center/clock] [right/controls]
        auto* grid = new QGridLayout(this);
        grid->setContentsMargins(10, 5, 10, 5);
       grid->setHorizontalSpacing(12);
        // Columns stretch equally around the center so the clock stays centered.
        grid->setColumnStretch(0, 1);   // left  space
       grid->setColumnStretch(1, 0);   // center (clock)
        grid->setColumnStretch(2, 1);   // right space (buttons live inside)

    // Status label only
    statusLabel = new QLabel("Standalone Mode", this);
    statusLabel->setStyleSheet("color: orange; font-size: 18px; padding-left: 5px;");
    grid->addWidget(statusLabel, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

    // Clock Label
    clockLabel = new QLabel(this);
    clockLabel->setAlignment(Qt::AlignCenter);
    clockLabel->setStyleSheet("color: white; padding: 5px;");
    grid->addWidget(clockLabel, 0, 1, Qt::AlignCenter);

    // Right control group (Playback + Settings), right-aligned as a unit
        QWidget* rightBox = new QWidget(this);
        auto* right = new QHBoxLayout(rightBox);
       right->setContentsMargins(0,0,0,0);
        right->setSpacing(12);

        // Playback Button (orange text)
        playbackButton = new QPushButton("▶ Playback", this);
        playbackButton->setStyleSheet(
            "QPushButton { color: orange; padding: 6px 13px; font-weight: 900; font-size: 18px; }"
            "QPushButton:hover { background-color: #444444; }"
            "QPushButton:pressed { background-color: #222222; }"
        );
        connect(playbackButton, &QPushButton::clicked, this, &Toolbar::playbackButtonClicked);
        right->addWidget(playbackButton, 0, Qt::AlignRight);

    // Settings Button
    settingsButton = new QPushButton("⚙ Settings", this);
    settingsButton->setStyleSheet(
        "QPushButton {"
        "   color: white;"
        "   padding: 6px 13px;"
        "   font-weight: 900;"
        "   font-size: 18px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #444444;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #222222;"
        "}"
    );
    right->addWidget(settingsButton, 0, Qt::AlignRight);
        grid->addWidget(rightBox, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
        setLayout(grid);

    // Connect Settings Button to Signal
    connect(settingsButton, &QPushButton::clicked, this, &Toolbar::settingsButtonClicked);

    // Start clock updates
    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &Toolbar::updateClock);
    clockTimer->start(1000);
    updateClock();

    // Setup network connectivity check
    connect(checkTimer, &QTimer::timeout, this, &Toolbar::checkInternetConnection);
    checkTimer->start(5000); // Check every 5 seconds
    checkInternetConnection(); // Initial check
}

void Toolbar::updateClock() {
    clockLabel->setText(QDateTime::currentDateTime().toString("dd MMM yyyy  HH:mm:ss AP"));
}

void Toolbar::checkInternetConnection() {
    QNetworkRequest request(QUrl("http://www.google.com"));
    QNetworkReply* reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError && SubscriptionManager::currentSubscriptionStatus()) {
            statusLabel->setText("Connected");
            statusLabel->setStyleSheet("color: green; font-size: 18px; padding-left: 5px;");
        } else {
            statusLabel->setText("Standalone Mode");
            statusLabel->setStyleSheet("color: orange; font-size: 18px; padding-left: 5px;");
        }
        reply->deleteLater();
    });
}
