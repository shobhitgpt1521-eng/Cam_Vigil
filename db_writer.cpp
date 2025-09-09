#include "db_writer.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

DbWriter::DbWriter(QObject* parent) : QObject(parent) {}
DbWriter::~DbWriter() {
    if (db_.isOpen()) db_.close();
}

bool DbWriter::openAt(const QString& dbFile) {
    if (db_.isOpen()) return true;
    QDir().mkpath(QFileInfo(dbFile).absolutePath());
    db_ = QSqlDatabase::addDatabase("QSQLITE", "camvigil_db");
    db_.setDatabaseName(dbFile);
    if (!db_.open()) {
        qWarning() << "[DB] open error:" << db_.lastError().text();
        return false;
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    return ensureSchema();
}

bool DbWriter::exec(const QString& sql) {
    QSqlQuery q(db_);
    if (!q.exec(sql)) {
        qWarning() << "[DB] SQL error:" << q.lastError().text() << " sql:" << sql;
        return false;
    }
    return true;
}

bool DbWriter::ensureSchema() {
    return
    exec("CREATE TABLE IF NOT EXISTS cameras ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " name TEXT, main_url TEXT UNIQUE, sub_url TEXT, created_at INTEGER DEFAULT (strftime('%s','now')) );") &&
    exec("CREATE TABLE IF NOT EXISTS sessions ("
         " id TEXT PRIMARY KEY, started_at INTEGER, archive_dir TEXT, segment_sec INTEGER );") &&
    exec("CREATE TABLE IF NOT EXISTS segments ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " session_id TEXT, camera_id INTEGER, camera_url TEXT,"
         " file_path TEXT UNIQUE, start_utc_ns INTEGER, end_utc_ns INTEGER,"
         " duration_ms INTEGER, size_bytes INTEGER, status INTEGER DEFAULT 0,"
         " FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE,"
         " FOREIGN KEY(camera_id) REFERENCES cameras(id) ON DELETE SET NULL );") &&
    exec("CREATE INDEX IF NOT EXISTS idx_segments_camera_time ON segments(camera_id,start_utc_ns);") &&
    exec("CREATE INDEX IF NOT EXISTS idx_segments_path ON segments(file_path);")&&
    exec("CREATE INDEX IF NOT EXISTS idx_segments_camera_url_time ON segments(camera_url, start_utc_ns);");
}

void DbWriter::ensureCamera(const QString& mainUrl, const QString& subUrl, const QString& name) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO cameras(name, main_url, sub_url) VALUES(?,?,?) "
              "ON CONFLICT(main_url) DO UPDATE SET name=excluded.name, sub_url=excluded.sub_url;");
    q.addBindValue(name);
    q.addBindValue(mainUrl);
    q.addBindValue(subUrl);
    if (!q.exec()) qWarning() << "[DB] ensureCamera:" << q.lastError().text();
}

void DbWriter::beginSession(const QString& sessionId, const QString& archiveDir, int segmentSec) {
    QSqlQuery q(db_);
    q.prepare("INSERT OR REPLACE INTO sessions(id, started_at, archive_dir, segment_sec)"
              " VALUES(?, strftime('%s','now'), ?, ?);");
    q.addBindValue(sessionId);
    q.addBindValue(archiveDir);
    q.addBindValue(segmentSec);
    if (!q.exec()) qWarning() << "[DB] beginSession:" << q.lastError().text();
}

static int cameraIdForUrl(QSqlDatabase& db, const QString& url) {
    QSqlQuery q(db);
    q.prepare("SELECT id FROM cameras WHERE main_url=?;");
    q.addBindValue(url);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

void DbWriter::addSegmentOpened(const QString& sessionId, const QString& cameraUrl,
                                const QString& filePath, qint64 startUtcNs) {
    const int camId = cameraIdForUrl(db_, cameraUrl);
    QSqlQuery q(db_);
    q.prepare("INSERT OR IGNORE INTO segments(session_id,camera_id,camera_url,file_path,start_utc_ns,status)"
              " VALUES(?,?,?,?,?,0);");
    q.addBindValue(sessionId);
    q.addBindValue(camId);
    q.addBindValue(cameraUrl);
    q.addBindValue(filePath);
    q.addBindValue(startUtcNs);
    if (!q.exec()) qWarning() << "[DB] addSegmentOpened:" << q.lastError().text();
}

void DbWriter::finalizeSegmentByPath(const QString& filePath, qint64 endUtcNs, qint64 durationMs) {
    const qint64 size = QFileInfo(filePath).exists() ? QFileInfo(filePath).size() : 0;
    QSqlQuery q(db_);
    q.prepare("UPDATE segments SET end_utc_ns=?, duration_ms=?, size_bytes=?, status=1 WHERE file_path=?;");
    q.addBindValue(endUtcNs);
    q.addBindValue(durationMs);
    q.addBindValue(size);
    q.addBindValue(filePath);
    if (!q.exec()) qWarning() << "[DB] finalizeSegment:" << q.lastError().text();
}

void DbWriter::markError(const QString& where, const QString& detail) {
    Q_UNUSED(where); Q_UNUSED(detail);
    // Hook for future 'events' table.
}
