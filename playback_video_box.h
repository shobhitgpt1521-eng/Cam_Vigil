#pragma once
#include <QWidget>
#include <QString>

// Lightweight display surface for playback; currently shows placeholder text.
// Later, swap to a video renderer (QVideoWidget/custom GL) without changing PlaybackWindow.
class PlaybackVideoBox : public QWidget {
    Q_OBJECT
public:
   explicit PlaybackVideoBox(QWidget* parent=nullptr);
   void setPlaceholder(const QString& text);

protected:
   void paintEvent(QPaintEvent*) override;
private:
    QString placeholder_ = QStringLiteral("Please select the camera and date");
};
