#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DbWriter : public QObject {
    Q_OBJECT
public:
    explicit DbWriter(QObject* parent=nullptr);
    ~DbWriter();

public slots:
    bool openAt(const QString& dbFile);
    void ensureCamera(const QString& mainUrl, const QString& subUrl, const QString& name);
    void beginSession(const QString& sessionId, const QString& archiveDir, int segmentSec);
    void addSegmentOpened(const QString& sessionId, const QString& cameraUrl,
                          const QString& filePath, qint64 startUtcNs);
    void finalizeSegmentByPath(const QString& filePath, qint64 endUtcNs, qint64 durationMs);
    void markError(const QString& where, const QString& detail);

private:
    bool ensureSchema();
    bool exec(const QString& sql);
    QSqlDatabase db_;
};
