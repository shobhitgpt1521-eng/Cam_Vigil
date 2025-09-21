#pragma once
#include <QWidget>
class QCheckBox; class QTimeEdit; class QLabel; class QPushButton;

class PlaybackTrimPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackTrimPanel(QWidget* parent=nullptr);
    void setEnabledPanel(bool on);                 // enable/disable controls (not the checkbox)
    void setRangeNs(qint64 start_ns, qint64 end_ns);
    void setDayStartNs(qint64 day_start_ns);       // to format HH:MM:SS
    void setDurationLabel(qint64 dur_ns);

signals:
    void trimModeToggled(bool on);
    void startEditedNs(qint64 ns_from_midnight);
    void endEditedNs(qint64 ns_from_midnight);
    void exportRequested();                        // we’ll pick up current range from window
private:
    qint64 dayStartNs_=0;
    QCheckBox *enableBox_;
    QTimeEdit *startEdit_, *endEdit_;
    QLabel *durLab_, *snapLab_;
    QPushButton *exportBtn_;
    qint64 timeEditToNs(const QTimeEdit*) const;
    void setTimeEdit(QTimeEdit*, qint64 ns);
};
