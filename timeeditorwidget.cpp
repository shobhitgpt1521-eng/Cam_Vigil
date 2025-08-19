#include "timeeditorwidget.h"
#include <QVBoxLayout>
#include <QProcess>
#include <QMessageBox>

TimeEditorWidget::TimeEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    dateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    dateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");

    timezoneCombo = new QComboBox(this);
    timezoneCombo->addItems({
        "Asia/Kolkata", "UTC", "Europe/London", "America/New_York", "Asia/Dubai"
    });

    applyButton = new QPushButton("Set System Time", this);
    connect(applyButton, &QPushButton::clicked, this, &TimeEditorWidget::onApplyClicked);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(dateTimeEdit);
    layout->addWidget(timezoneCombo);
    layout->addWidget(applyButton);
    setLayout(layout);
}

void TimeEditorWidget::onApplyClicked()
{
    QDateTime dt = dateTimeEdit->dateTime();
    QString timezone = timezoneCombo->currentText();
    QString datetimeStr = dt.toString("yyyy-MM-dd HH:mm:ss");

    int code1 = QProcess::execute("timedatectl", { "set-time", datetimeStr });
    int code2 = QProcess::execute("timedatectl", { "set-timezone", timezone });

    if (code1 == 0 && code2 == 0)
        QMessageBox::information(this, "Success", "Time updated successfully.");
    else
        QMessageBox::warning(this, "Error", "Failed to update time. Check snap permissions.");
}
