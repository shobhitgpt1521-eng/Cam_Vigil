QT       += core gui
QT += widgets multimedia multimediawidgets opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# Disable deprecated Qt APIs before version 6.0.0
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# Use pkg-config to handle OpenCV and GStreamer includes/libs
CONFIG += link_pkgconfig
PKGCONFIG += opencv4 gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0 glib-2.0 gstreamer-gl-1.0

# Added linker flags for libudev (required for hotplug support)
LIBS += -Wl,--no-as-needed -ludev -Wl,--as-needed

LIBS += -lGL

LIBS     += -lgstreamer-1.0 -lgstvideo-1.0 -lgstapp-1.0 -lgstbase-1.0 -lgobject-2.0 -lglib-2.0

QT       += dbus concurrent

SOURCES += \
    archivemanager.cpp \
    archivewidget.cpp \
    archiveworker.cpp \
    cameradetailswidget.cpp \
    cameramanager.cpp \
    camerastreams.cpp \
    fullscreenviewer.cpp \
    layoutmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    navbar.cpp \
    operationstatuswidget.cpp \
    settingswindow.cpp \
    storagedetailswidget.cpp \
    streammanager.cpp \
    streamworker.cpp \
    subscriptionmanager.cpp \
    timeeditorwidget.cpp \
    toolbar.cpp \
    videoplayerwindow.cpp

HEADERS += \
    archivemanager.h \
    archivewidget.h \
    archiveworker.h \
    cameradetailswidget.h \
    cameramanager.h \
    camerastreams.h \
    clickablelabel.h \
    fullscreenviewer.h \
    glcontainerwidget.h \
    layoutmanager.h \
    mainwindow.h \
    navbar.h \
    operationstatuswidget.h \
    settingswindow.h \
    storagedetailswidget.h \
    streammanager.h \
    streamworker.h \
    subscriptionmanager.h \
    timeeditorwidget.h \
    toolbar.h \
    videoplayerwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    spinner.qrc \
    splash.qrc
