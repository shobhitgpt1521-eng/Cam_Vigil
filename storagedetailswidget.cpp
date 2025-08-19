#include "storagedetailswidget.h"
#include <QFormLayout>
#include <QStorageInfo>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include <QTimer>
#include <QDateTime>

StorageDetailsWidget::StorageDetailsWidget(ArchiveManager* archiveManager, QWidget *parent)
    : QWidget(parent), archiveManager(archiveManager)
{
    QFormLayout* formLayout = new QFormLayout(this);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignHCenter);

    storageDeviceStatusLabel = new QLabel(this);
    storageDeviceStatusLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    formLayout->addRow(new QLabel("Storage Device"), storageDeviceStatusLabel);

    storageProgressBar = new QProgressBar(this);
    storageProgressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    storageProgressBar->setMaximum(100);
    formLayout->addRow(new QLabel("Capacity"), storageProgressBar);

    capacityDetailsLabel = new QLabel(this);
    capacityDetailsLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    formLayout->addRow(new QLabel(""), capacityDetailsLabel);

    durationCombo = new QComboBox(this);
    durationCombo->addItem("1 min", 60);
    durationCombo->addItem("5 mins", 300);
    durationCombo->addItem("15 mins", 900);
    durationCombo->addItem("30 mins", 1800);
    durationCombo->addItem("60 mins", 3600);
    durationCombo->setCurrentIndex(1);
    durationCombo->setStyleSheet("font-size: 18px; font-weight: bold; color: white; background-color: #4d4d4d;");
    durationCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StorageDetailsWidget::onDurationChanged);
    formLayout->addRow(new QLabel("Recording Duration"), durationCombo);

    setLayout(formLayout);
    updateStorageInfo();

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &StorageDetailsWidget::updateStorageInfo);
    refreshTimer->start(3000);
}

void StorageDetailsWidget::onDurationChanged(int index) {
    int seconds = durationCombo->itemData(index).toInt();
    emit segmentDurationChanged(seconds);
    if (archiveManager) {
        archiveManager->updateSegmentDuration(seconds);
    }
}

void StorageDetailsWidget::updateStorageInfo() {
    QString externalPath = archiveManager->findExternalStoragePath();
    if (externalPath.isEmpty()) {
        storageDeviceStatusLabel->setText("No Storage Device Connected");
        capacityDetailsLabel->setText("");
        storageProgressBar->setValue(0);
    } else {
        storageDeviceStatusLabel->setText(QString("External Device: %1").arg(externalPath));
        QStorageInfo storage(externalPath);
        qint64 totalGB = storage.bytesTotal() / (1024 * 1024 * 1024);
        qint64 availableGB = storage.bytesAvailable() / (1024 * 1024 * 1024);
        qint64 usedGB = totalGB - availableGB;
        capacityDetailsLabel->setText(QString("%1 GB Used | %2 GB Available").arg(usedGB).arg(availableGB));
        int usedPercent = (totalGB > 0) ? (usedGB * 100) / totalGB : 0;
        storageProgressBar->setValue(usedPercent);
        storageProgressBar->setFormat(QString("%1% Used").arg(usedPercent));
        storageProgressBar->setStyleSheet(R"(
            QProgressBar {
                border: 1px solid #FFFFFF;
                background-color: #111;
                text-align: center;
                color: #383838;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #FFFFFF;
            }
        )");

        if (usedPercent >= 95) {
            QDateTime current = QDateTime::currentDateTime();
            if (!lastCleanupTime.isValid() || lastCleanupTime.msecsTo(current) > 60000) {
                qDebug() << "Storage is" << usedPercent << "% full. Requesting cleanup.";
                emit requestCleanup();
                lastCleanupTime = current;
            }
        }
    }
}
