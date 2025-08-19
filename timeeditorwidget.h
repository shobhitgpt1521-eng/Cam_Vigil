#ifndef TIMEEDITORWIDGET_H
#define TIMEEDITORWIDGET_H


#include <QWidget>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QPushButton>

class TimeEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimeEditorWidget(QWidget *parent = nullptr);

private slots:
    void onApplyClicked();

private:
    QDateTimeEdit* dateTimeEdit;
    QComboBox* timezoneCombo;
    QPushButton* applyButton;
};

#endif // TIMEEDITORWIDGET_H
