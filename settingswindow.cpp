#include "settingswindow.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QSurfaceFormat>
#include <QOpenGLFunctions>
#include <QFormLayout>
#include "operationstatuswidget.h"
#include "cameradetailswidget.h"
#include "storagedetailswidget.h"
#include "archivewidget.h"
#include "cameramanager.h"

SettingsWindow::SettingsWindow(ArchiveManager* archiveManager,
                               CameraManager* cameraManager,
                               QWidget *parent)
    : QOpenGLWidget(parent)
    , archiveManager(archiveManager)
    , cameraManager(cameraManager)
{
    //  we use the default OpenGL format set in main()
    setFormat(QSurfaceFormat::defaultFormat());

    setWindowTitle("Settings");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setGeometry(QGuiApplication::primaryScreen()->geometry());


    this->setStyleSheet(R"(
        QWidget {
            background-color: #000000;
            color: #FFFFFF;
            font-family: "Courier New", monospace;
            font-size: 14px;
        }
        QLabel { padding: 4px; }
        QLineEdit, QComboBox {
            background-color: #111;
            border: 1px solid #FFFFFF;
            color: #FFFFFF;
            padding: 4px;
        }
        QPushButton {
            background-color: #111;
            border: 1px solid #FFFFFF;
            padding: 5px 10px;
            color: #FFFFFF;
        }
        QPushButton:hover { background-color: #222; }
        QListWidget {
            background-color: #111;
            border: 1px solid #FFFFFF;
            color: #00BFFF;
        }
        QProgressBar {
            border: 1px solid #FFFFFF;
            background-color: #111;
            text-align: center;
        }
        QProgressBar::chunk { background-color: #FFFFFF; }
    )");

    // Main vertical layout on this GL widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    // Scrollable area for all settings content
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("border: none; background-color: black;");
    mainLayout->addWidget(scrollArea);

    QWidget* scrollContent = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(20);
    scrollContent->setStyleSheet("background-color: black;");
    scrollArea->setWidget(scrollContent);

    // ─── Navbar ─────────────────────────────────────────────────────────────
    navbar = new QWidget(scrollContent);
    navbar->setStyleSheet("background-color: #2C2C2C; padding: 5px;");
    navbar->setFixedHeight(50);
    QHBoxLayout* navbarLayout = new QHBoxLayout(navbar);
    navbarLayout->setContentsMargins(0, 0, 0, 0);
    navbarLayout->setSpacing(0);

    QPushButton* backButton = new QPushButton("← Back", navbar);
    backButton->setStyleSheet("color: white; background: transparent; font-size: 16px;");
    backButton->setCursor(Qt::PointingHandCursor);
    connect(backButton, &QPushButton::clicked, this, &SettingsWindow::close);
    navbarLayout->addWidget(backButton, 0, Qt::AlignLeft);

    QLabel* titleLabel = new QLabel("CamVigil-Settings", navbar);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 20px; padding: 5px;");
    navbarLayout->addWidget(titleLabel, 1, Qt::AlignCenter);

    closeIconButton = new QPushButton("✖", navbar);
    closeIconButton->setStyleSheet("background: transparent; border: none; color: white; font-size: 18px; padding: 5px;");
    closeIconButton->setFixedSize(30, 30);
    connect(closeIconButton, &QPushButton::clicked, this, &SettingsWindow::closeWindow);
    navbarLayout->addWidget(closeIconButton, 0, Qt::AlignRight);

    navbar->setLayout(navbarLayout);
    scrollLayout->addWidget(navbar, 0, Qt::AlignTop);

    // ─── Operation Status ───────────────────────────────────────────────────
    OperationStatusWidget* operationWidget = new OperationStatusWidget();
    scrollLayout->addWidget(operationWidget, 0, Qt::AlignLeft);

    // ─── Camera Details ─────────────────────────────────────────────────────
    CameraDetailsWidget* cameraWidget = new CameraDetailsWidget(cameraManager);
    cameraWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scrollLayout->addWidget(cameraWidget, 0, Qt::AlignLeft);

    // ─── Time Settings ───────────────────────────────────────────────────────
    QFormLayout* timeLayout = new QFormLayout();
    timeLayout->setLabelAlignment(Qt::AlignLeft);
    timeLayout->setFormAlignment(Qt::AlignHCenter);

    QLabel* systemTimeLabel = new QLabel("System Time");
    systemTimeLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");

    TimeEditorWidget* timeEditor = new TimeEditorWidget();
    timeEditor->setStyleSheet("background-color: #4d4d4d;");
    timeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    timeLayout->addRow(systemTimeLabel, timeEditor);

    // Wrap in a QWidget to insert into scrollLayout
    QWidget* timeWidgetWrapper = new QWidget(scrollContent);
    timeWidgetWrapper->setLayout(timeLayout);
    scrollLayout->addWidget(timeWidgetWrapper, 0, Qt::AlignLeft);


    // ─── Storage Details ────────────────────────────────────────────────────
    StorageDetailsWidget* storageWidget = new StorageDetailsWidget(archiveManager);
    storageWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scrollLayout->addWidget(storageWidget, 0, Qt::AlignLeft);
    connect(storageWidget, &StorageDetailsWidget::requestCleanup,
            archiveManager, &ArchiveManager::cleanupArchive);
    connect(archiveManager, &ArchiveManager::segmentWritten,
            storageWidget, &StorageDetailsWidget::updateStorageInfo);

    scrollLayout->addStretch();

    // ─── Archive List ───────────────────────────────────────────────────────
    QLabel* archiveTitle = new QLabel("Archives", scrollContent);
    archiveTitle->setAlignment(Qt::AlignCenter);
    archiveTitle->setStyleSheet("font-size: 22px; font-weight: bold; color: white;");
    scrollLayout->addWidget(archiveTitle);

    archiveWidget = new ArchiveWidget(cameraManager, archiveManager);
    archiveWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    archiveWidget->setMinimumHeight(400);
    scrollLayout->addWidget(archiveWidget, 1);

    scrollLayout->addStretch();
}

void SettingsWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    qDebug() << "[SW] OpenGL initialized:";
    qDebug() << "    Vendor:  " << reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    qDebug() << "    Renderer:" << reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

void SettingsWindow::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void SettingsWindow::closeWindow()
{
    close();
}
