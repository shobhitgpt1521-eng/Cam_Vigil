#include "videoplayerwindow.h"

#include <QFileInfo>
#include <QTime>
#include <QDebug>
#include <QShowEvent>
#include <QResizeEvent>
#include <QEvent>

#include <gst/video/videooverlay.h>   // gst_video_overlay_*

// -------------------- Bus thread --------------------
void GstBusThread::run() {
    if (!pipeline) return;
    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus) return;

    for (;;) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_MSECOND * 200);
        if (!msg) {
            if (isInterruptionRequested()) break;
            continue;
        }

        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr; gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            emit gstError(QString("GStreamer error: %1 (%2)")
                          .arg(err ? err->message : "unknown")
                          .arg(dbg ? dbg : ""));
            g_clear_error(&err); g_free(dbg);
            break;
        }
        case GST_MESSAGE_EOS:
            emit gstEos();
            break;
        default:
            break;
        }

        gst_message_unref(msg);
        if (isInterruptionRequested()) break;
    }

    gst_object_unref(bus);
}

// -------------------- helpers --------------------
static inline GstElement* try_make(const char* factory) {
    return gst_element_factory_make(factory, nullptr);
}

// Recursively find the first element in a bin that implements GstVideoOverlay
static GstElement* find_overlay_in_bin(GstElement *elem) {
    if (!elem) return nullptr;
    if (GST_IS_VIDEO_OVERLAY(elem)) return elem;
    if (GST_IS_BIN(elem)) {
        GList *children = GST_BIN_CHILDREN(GST_BIN(elem));
        for (GList *l = children; l; l = l->next) {
            GstElement *child = GST_ELEMENT(l->data);
            if (!child) continue;
            if (GstElement *ov = find_overlay_in_bin(child)) return ov;
        }
    }
    return nullptr;
}

// -------------------- VideoPlayerWindow --------------------
VideoPlayerWindow::VideoPlayerWindow(const QString& filePath, QWidget *parent)
    : QWidget(parent)
{
    gst_init(nullptr, nullptr);

    setWindowTitle("Video Playback");
    resize(960, 600);
    setStyleSheet("background-color: black;");

    auto *mainLayout = new QVBoxLayout(this);

    QFileInfo fi(filePath);
    const QString fileName    = fi.fileName();
    const QString fileDateTime= fi.lastModified().toString("yyyy-MM-dd hh:mm:ss");

    fileInfoLabel = new QLabel(QString("📂 %1\n📅 %2").arg(fileName, fileDateTime), this);
    fileInfoLabel->setStyleSheet("color: white; font-size: 14px; padding: 10px;");
    fileInfoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(fileInfoLabel);

    // Video area (must be native to get a window handle)
    videoArea = new QWidget(this);
    videoArea->setStyleSheet("background:black;");
    videoArea->setAttribute(Qt::WA_NativeWindow);
    mainLayout->addWidget(videoArea, /*stretch*/1);

    // Minimal controls: Play/Pause + time label
    auto *controlsLayout = new QHBoxLayout();

    playPauseButton = new QPushButton("⏸ Pause", this);
    connect(playPauseButton, &QPushButton::clicked, this, &VideoPlayerWindow::playPauseVideo);
    controlsLayout->addWidget(playPauseButton);

    controlsLayout->addStretch();

    timeLabel = new QLabel("00:00 / 00:00", this);
    timeLabel->setStyleSheet("color: white;");
    controlsLayout->addWidget(timeLabel);

    mainLayout->addLayout(controlsLayout);

    closeButton = new QPushButton("❌ Close", this);
    connect(closeButton, &QPushButton::clicked, this, &VideoPlayerWindow::close);
    mainLayout->addWidget(closeButton, 0, Qt::AlignCenter);

    setLayout(mainLayout);

    // UI update timer (position)
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &VideoPlayerWindow::updateElapsedTime);

    // Build and start pipeline
    initPipeline(filePath);

    // Bus thread so messages are serviced off the GUI thread
    busThread = new GstBusThread(this);
    busThread->setPipeline(pipeline);
    connect(busThread, &GstBusThread::gstError, this, &VideoPlayerWindow::onGstError);
    connect(busThread, &GstBusThread::gstEos,   this, &VideoPlayerWindow::onGstEos);
    busThread->start();
}

void VideoPlayerWindow::initPipeline(const QString &filePath) {
    // --- Elements ---
    GstElement *filesrc = try_make("filesrc");
    g_object_set(filesrc, "location", filePath.toUtf8().constData(), NULL);

    GstElement *demux   = try_make("matroskademux");
    GstElement *parser  = try_make("h264parse");

    // Prefer Intel VAAPI; fallback to software if not available
    GstElement *decoder = try_make("vaapih264dec");
    bool usingVaapi = (decoder != nullptr);
    if (!decoder) decoder = try_make("avdec_h264");

    // Threading queues for smoother decode/display
    GstElement *q1 = try_make("queue");  // after parser
    GstElement *q2 = try_make("queue");  // after decoder

    // Convert to CPU/system memory for stable X11 sink
    GstElement *vconv = try_make("videoconvert");

    // X11 sink (overlay-capable, embeds into our widget)
    GstElement *sink = try_make("ximagesink");

    if (!filesrc || !demux || !parser || !decoder || !vconv || !sink) {
        qWarning() << "Failed to create required GStreamer elements";
        return;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sink), "force-aspect-ratio"))
        g_object_set(sink, "force-aspect-ratio", TRUE, NULL);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sink), "sync"))
        g_object_set(sink, "sync", TRUE, NULL);

    // --- Pipeline ---
    pipeline = gst_pipeline_new("archive-player");
    gst_bin_add_many(GST_BIN(pipeline), filesrc, demux, parser, NULL);
    if (q1) gst_bin_add(GST_BIN(pipeline), q1);
    gst_bin_add(GST_BIN(pipeline), decoder);
    if (q2) gst_bin_add(GST_BIN(pipeline), q2);
    gst_bin_add_many(GST_BIN(pipeline), vconv, sink, NULL);

    // Static links
    gst_element_link(filesrc, demux);
    if (q1) gst_element_link_many(parser, q1, decoder, NULL);
    else    gst_element_link_many(parser, decoder, NULL);
    if (q2) gst_element_link_many(decoder, q2, vconv, sink, NULL);
    else    gst_element_link_many(decoder, vconv, sink, NULL);

    // Dynamic pad (demux -> parser), only link VIDEO pads
    g_signal_connect(demux, "pad-added",
        G_CALLBACK(+[] (GstElement*, GstPad *pad, gpointer data){
            auto *parser = static_cast<GstElement*>(data);
            GstCaps *caps = gst_pad_query_caps(pad, nullptr);
            bool isVideo = false;
            if (caps) {
                const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
                if (name && g_str_has_prefix(name, "video/")) isVideo = true;
                gst_caps_unref(caps);
            }
            if (!isVideo) return;
            GstPad *sinkPad = gst_element_get_static_pad(parser, "sink");
            if (!gst_pad_is_linked(sinkPad)) gst_pad_link(pad, sinkPad);
            gst_object_unref(sinkPad);
        }), parser);

    // Install sync handler BEFORE preroll so we answer prepare-window-handle in time
    installBusSyncHandler();

    // 1) PREROLL → we want caps/size negotiated before binding overlay
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    GstStateChangeReturn preroll = gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND * 3);
    qDebug() << "[Player] Preroll state:" << preroll
             << "Decoder:" << (usingVaapi ? "vaapih264dec" : "avdec_h264");

    // 2) Find overlay-capable element (ximagesink itself)
    videoSink = find_overlay_in_bin(sink);

    // Safety: ensure it’s bound and rectangle applied (prepare-window-handle should also do it)
    bindOverlay();
    applyRenderRect();

    // 3) PLAY
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    isPlaying = true;
    playPauseButton->setText("⏸ Pause");

    // Duration + start UI timer
    queryDuration();
    updateTimer->start(250);
}

// -------------------- Bus sync handler --------------------
GstBusSyncReply VideoPlayerWindow::onBusSync(GstBus * /*bus*/, GstMessage *msg, gpointer user_data) {
    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT) return GST_BUS_PASS;
    if (!gst_is_video_overlay_prepare_window_handle_message(msg)) return GST_BUS_PASS;

    auto *self = static_cast<VideoPlayerWindow*>(user_data);
    if (!self || !self->videoSink || !self->videoArea) return GST_BUS_PASS;

    // Ensure child XID exists
    (void)self->videoArea->winId();

    gst_video_overlay_set_window_handle(
        GST_VIDEO_OVERLAY(self->videoSink),
        (guintptr)self->videoArea->winId()
    );

    // Apply the current rectangle immediately
    self->applyRenderRect();

    return GST_BUS_DROP; // we handled it
}

void VideoPlayerWindow::installBusSyncHandler() {
    if (!pipeline) return;
    GstBus *bus = gst_element_get_bus(pipeline);
    if (bus) {
        gst_bus_set_sync_handler(bus, (GstBusSyncHandler)&VideoPlayerWindow::onBusSync, this, nullptr);
        gst_object_unref(bus);
    }
}

// -------------------- Overlay binding & sizing --------------------
void VideoPlayerWindow::bindOverlay() {
    if (!videoSink || !videoArea) return;
    // ensure native handle exists
    (void)videoArea->winId();
    if (GST_IS_VIDEO_OVERLAY(videoSink)) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videoSink),
                                            (guintptr)videoArea->winId());
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(videoSink));
    }
}

void VideoPlayerWindow::applyRenderRect() {
    if (!videoSink || !videoArea) return;
    if (!GST_IS_VIDEO_OVERLAY(videoSink)) return;

    // X11 ximagesink expects coordinates in widget pixels (no DPR scaling)
    const int x = 0;
    const int y = 0;
    const int w = videoArea->width();
    const int h = videoArea->height();

    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videoSink), x, y, w, h);
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(videoSink));
}

bool VideoPlayerWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == videoArea && (ev->type() == QEvent::Resize || ev->type() == QEvent::Show)) {
        applyRenderRect();
    }
    return QWidget::eventFilter(obj, ev);
}

void VideoPlayerWindow::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    // Track direct size changes of the child native window
    if (videoArea) videoArea->installEventFilter(this);
    bindOverlay();
    applyRenderRect();
}

void VideoPlayerWindow::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    applyRenderRect();
}

// -------------------- Media controls & housekeeping --------------------
void VideoPlayerWindow::queryDuration() {
    if (!pipeline) return;
    gint64 durNs = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &durNs)) {
        durationMs = durNs / GST_MSECOND;
        const QString total = QTime(0,0).addMSecs(durationMs).toString("mm:ss");
        timeLabel->setText(QString("00:00 / %1").arg(total));
    }
}

void VideoPlayerWindow::playPauseVideo() {
    if (!pipeline) return;
    gst_element_set_state(pipeline, isPlaying ? GST_STATE_PAUSED : GST_STATE_PLAYING);
    playPauseButton->setText(isPlaying ? "▶ Play" : "⏸ Pause");
    isPlaying = !isPlaying;
}

void VideoPlayerWindow::updateElapsedTime() {
    if (!pipeline) return;
    gint64 posNs = 0;
    if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &posNs)) {
        const qint64 posMs = posNs / GST_MSECOND;
        const QString cur = QTime(0,0).addMSecs(posMs).toString("mm:ss");
        const QString tot = QTime(0,0).addMSecs(durationMs).toString("mm:ss");
        timeLabel->setText(QString("%1 / %2").arg(cur, tot));
    }
}

void VideoPlayerWindow::onGstError(QString msg) {
    qWarning() << msg;
}

void VideoPlayerWindow::onGstEos() {
    qDebug() << "Playback reached EOS";
    // Optional: close player or loop.
}

VideoPlayerWindow::~VideoPlayerWindow() {
    if (busThread) {
        busThread->requestInterruption();
        busThread->wait(300);
    }
    cleanupPipeline();
}

void VideoPlayerWindow::cleanupPipeline() {
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
        videoSink = nullptr;
    }
}
