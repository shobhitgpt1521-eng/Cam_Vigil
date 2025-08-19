#include "toolbar.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include "subscriptionmanager.h"

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

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);

    // Status label only
    statusLabel = new QLabel("Standalone Mode", this);
    statusLabel->setStyleSheet("color: orange; font-size: 18px; padding-left: 5px;");
    layout->addWidget(statusLabel, 0, Qt::AlignLeft);

    layout->addStretch();

    // Clock Label
    clockLabel = new QLabel(this);
    clockLabel->setAlignment(Qt::AlignCenter);
    clockLabel->setStyleSheet("color: white; padding: 5px;");
    layout->addWidget(clockLabel, 0, Qt::AlignCenter);

    layout->addStretch();

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
    layout->addWidget(settingsButton, 0, Qt::AlignRight);

    setLayout(layout);

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
