#pragma once
#include <QWidget>
class QPushButton;

class PlaybackSideControls : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackSideControls(QWidget* parent=nullptr);
    QPushButton* playButton() const { return play_; }
    QPushButton* pauseButton() const { return pause_; }
    void setEnabledControls(bool on);

signals:
     void playClicked();
    void pauseClicked();

private:
    QPushButton* play_{nullptr};
    QPushButton* pause_{nullptr};
};
