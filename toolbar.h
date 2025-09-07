#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QNetworkAccessManager>

class Toolbar : public QWidget {
    Q_OBJECT

public:
    explicit Toolbar(QWidget* parent = nullptr);

signals:
    void settingsButtonClicked();
    void playbackButtonClicked();
private slots:
    void updateClock();
    void checkInternetConnection();

private:
    QLabel* clockLabel;
    QPushButton* settingsButton;
    QLabel* statusLabel;
    QTimer* clockTimer;
    QNetworkAccessManager* networkManager;
    QTimer* checkTimer;
    QPushButton* playbackButton;
};

#endif // TOOLBAR_H
