#ifndef VIDEOPLAYERWINDOW_H
#define VIDEOPLAYERWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QThread>
#include <QResizeEvent>
#include <QEvent>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

class GstBusThread : public QThread {
    Q_OBJECT
public:
    explicit GstBusThread(QObject *parent=nullptr) : QThread(parent) {}
    void setPipeline(GstElement *p) { pipeline = p; }
    void run() override;
signals:
    void gstError(QString msg);
    void gstEos();
private:
    GstElement *pipeline = nullptr;
};

class VideoPlayerWindow : public QWidget {
    Q_OBJECT
public:
    explicit VideoPlayerWindow(const QString& filePath, QWidget *parent = nullptr);
    ~VideoPlayerWindow() override;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void playPauseVideo();
    void updateElapsedTime();
    void onGstError(QString msg);
    void onGstEos();

private:
    void initPipeline(const QString &filePath);
    void installBusSyncHandler();
    static GstBusSyncReply onBusSync(GstBus *bus, GstMessage *msg, gpointer user_data);
    void bindOverlay();
    void cleanupPipeline();
    void queryDuration();
    void applyRenderRect();
    // GStreamer
    GstElement *pipeline = nullptr;
    GstElement *videoSink = nullptr;
    GstBusThread *busThread = nullptr;

    // UI
    QWidget *videoArea = nullptr;
    QLabel *fileInfoLabel = nullptr;
    QPushButton *playPauseButton = nullptr;
    QLabel *timeLabel = nullptr;        // shows "mm:ss / mm:ss"
    QPushButton *closeButton = nullptr;
    QTimer *updateTimer = nullptr;

    // State
    bool isPlaying = true;
    qint64 durationMs = 0;
};

#endif // VIDEOPLAYERWINDOW_H
