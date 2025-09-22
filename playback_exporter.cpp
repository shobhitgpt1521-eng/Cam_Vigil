#include "playback_exporter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDate>
#include <QTextStream>

static inline double secFromNs(qint64 ns){ return double(ns)/1e9; }

PlaybackExporter::PlaybackExporter(QObject* p): QObject(p) {}

void PlaybackExporter::setPlaylist(const QVector<PlaybackSegmentIndex::FileSeg>& pl, qint64 dayStartNs){
    playlist_ = pl; dayStartNs_ = dayStartNs;
}
void PlaybackExporter::setSelection(qint64 s, qint64 e){
    selStartNs_ = s; selEndNs_ = e;
}
void PlaybackExporter::setOptions(const ExportOptions& o){ opts_ = o; }

void PlaybackExporter::cancel(){ abort_.store(true); }

void PlaybackExporter::start(){
    emit log("[Export] start");
    if (selEndNs_ <= selStartNs_) { emit error("Invalid selection"); return; }
    if (playlist_.isEmpty()) { emit error("No playlist"); return; }
    if (!ensureOutDir_(nullptr)) { emit error("Cannot create output directory"); return; }

    const auto parts = computeParts_();
    if (parts.isEmpty()) { emit error("Selection overlaps no files"); return; }

    QTemporaryDir tmp;
    if (!tmp.isValid()) { emit error("Temp directory creation failed"); return; }
    emit log(QString("[Export] tmp: %1").arg(tmp.path()));

    QStringList cutPaths;
    if (!cutParts_(parts, &cutPaths)) {
        emit error("Cut failed"); return;
    }
    if (abort_.load()) { emit error("Canceled"); return; }

    QString listPath;
    if (!writeConcatList_(cutPaths, &listPath)) {
        emit error("Concat list write failed"); return;
    }

    const QString outPath = uniqueOutPath_();
    if (!concat_(listPath, outPath)) {
        emit error("Concat failed"); return;
    }
    if (abort_.load()) { emit error("Canceled"); return; }

    emit progress(100.0);
    emit log(QString("[Export] OK -> %1").arg(outPath));
    emit finished(outPath);
}

QVector<ClipPart> PlaybackExporter::computeParts_() const {
    QVector<ClipPart> out;
    const qint64 selAbsA = dayStartNs_ + selStartNs_;
    const qint64 selAbsB = dayStartNs_ + selEndNs_;
    for (const auto& fs : playlist_) {
        const qint64 a = std::max(fs.start_ns, selAbsA);
        const qint64 b = std::min(fs.end_ns,   selAbsB);
        if (b > a) out.push_back({ fs.path, a - fs.start_ns, b - fs.start_ns });
        if (fs.end_ns >= selAbsB) break;
    }
    return out;
}

bool PlaybackExporter::ensureOutDir_(QString* err) const {
    QDir d(opts_.outDir);
    if (d.exists()) return true;
    if (QDir().mkpath(opts_.outDir)) return true;
    if (err) *err = "mkpath failed";
    return false;
}

QString PlaybackExporter::uniqueOutPath_() const {
    const QString base = opts_.baseName.isEmpty()
        ? QString("CamVigil_%1").arg(QDate::currentDate().toString("yyyy-MM-dd"))
        : opts_.baseName;
    QString out = QDir(opts_.outDir).filePath(base + ".mp4");
    int i=1;
    while (QFile::exists(out)) out = QDir(opts_.outDir).filePath(QString("%1(%2).mp4").arg(base).arg(++i));
    return out;
}

bool PlaybackExporter::runFfmpeg_(const QStringList& args, QByteArray* errOut){
    if (abort_.load()) return false;
    QProcess p;
    p.setProgram(opts_.ffmpegPath);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    if (!p.waitForStarted()) return false;

    // Poll for cancel and progress
    while (p.state() == QProcess::Running) {
        if (abort_.load()) {
            p.kill();
            p.waitForFinished();
            return false;
        }
        p.waitForReadyRead(50);
        // Optionally parse stderr for progress here
    }
    if (errOut) *errOut = p.readAllStandardError();
    return p.exitStatus()==QProcess::NormalExit && p.exitCode()==0;
}

bool PlaybackExporter::cutParts_(const QVector<ClipPart>& parts, QStringList* cutPaths){
    const int N = parts.size();
    cutPaths->reserve(N);
    for (int i=0;i<N;++i) {
        if (abort_.load()) return false;
        const auto& part = parts[i];
        const double ss = secFromNs(part.inStartNs);
        const double to = secFromNs(part.inEndNs);
        const QString cut = QString("%1/part_%2.mkv").arg(QDir::tempPath()).arg(i,4,10,QChar('0'));
        cutPaths->push_back(cut);

        QStringList args; args << "-hide_banner" << "-y";

        if (opts_.precise) {
            const double coarse = std::max(0.0, ss - 3.0); // jump ~3s earlier to reduce decode cost
            args << "-ss" << QString::number(coarse, 'f', 3)      // coarse input seek
                 << "-i"  << part.path
                 << "-ss" << QString::number(ss - coarse, 'f', 6) // fine output seek
                 << "-to" << QString::number(to - coarse, 'f', 6)
                 << "-c:v" << opts_.vcodec
                 << "-preset" << opts_.preset
                 << "-crf" << QString::number(opts_.crf)
                 << "-pix_fmt" << "yuv420p"
                 << "-fflags" << "+genpts"
                 << "-reset_timestamps" << "1";
            if (opts_.copyAudio) args << "-c:a" << "copy";
            else                 args << "-c:a" << "aac" << "-b:a" << "128k";
            args << "-movflags" << "+faststart"
                 << cut;
        } else {
            args << "-ss" << QString::number(ss, 'f', 6)
                 << "-to" << QString::number(to, 'f', 6)
                 << "-i"  << part.path
                 << "-c"  << "copy"
                 << "-avoid_negative_ts" << "make_zero"
                 << cut;
        }

        QByteArray err;
        emit log(QString("[Export] cut %1/%2").arg(i+1).arg(N));
        if (!runFfmpeg_(args, &err)) { emit log(QString::fromUtf8(err)); return false; }
        emit progress( (i+1) * 100.0 / (N+1) );
    }
    return true;
}


bool PlaybackExporter::writeConcatList_(const QStringList& cutPaths, QString* listPath){
    const QString p = QDir::temp().absoluteFilePath("concat_inputs.txt");
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) return false;
    QTextStream ts(&f);
    for (const auto& cp : cutPaths) {
        ts << "file '" << QFileInfo(cp).absoluteFilePath().replace('\'',"\\'") << "'\n";
    }
    f.close();
    *listPath = p;
    return true;
}

bool PlaybackExporter::concat_(const QString& listPath, const QString& outPath){
    QStringList args;
    args << "-hide_banner" << "-y"
         << "-f" << "concat" << "-safe" << "0"
         << "-i" << listPath;

    if (opts_.precise) {
        args << "-c:v" << opts_.vcodec
             << "-preset" << opts_.preset
             << "-crf" << QString::number(opts_.crf);
        if (opts_.copyAudio) args << "-c:a" << "copy";
    } else {
        args << "-c" << "copy";
    }
    args << outPath;

    QByteArray err;
    emit log("[Export] concat");
    const bool ok = runFfmpeg_(args, &err);
    if (!ok) emit log(QString::fromUtf8(err));
    return ok;
}
