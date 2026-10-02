#include "core/ops/checksumtask.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {
constexpr qint64 kChunkSize = 1 << 20; // 1 MiB

struct PendingFile
{
    QString absolutePath;
    QString displayName;
};
} // namespace

ChecksumTask::ChecksumTask(QStringList paths, QSharedPointer<QAtomicInt> cancelled)
    : m_paths(std::move(paths))
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

void ChecksumTask::run()
{
    bool wasCancelled = false;

    // Expand any directories in the selection into the files they
    // contain, recursively -- a folder used to just come back as
    // "(error)" since QFile can't open a directory for reading.
    QVector<PendingFile> files;
    for (const QString &selectedPath : m_paths) {
        if (m_cancelled->loadRelaxed() != 0) {
            wasCancelled = true;
            break;
        }

        const QFileInfo info(selectedPath);
        if (info.isDir()) {
            const QDir baseDir(info.absoluteFilePath());
            const QString folderName = info.fileName();
            QDirIterator it(info.absoluteFilePath(), QDir::Files | QDir::NoDotAndDotDot,
                             QDirIterator::Subdirectories);
            while (it.hasNext()) {
                if (m_cancelled->loadRelaxed() != 0) {
                    wasCancelled = true;
                    break;
                }
                const QString filePath = it.next();
                const QString relative = baseDir.relativeFilePath(filePath);
                files.append({filePath, folderName + QLatin1Char('/') + relative});
            }
        } else {
            files.append({info.absoluteFilePath(), info.fileName()});
        }

        if (wasCancelled)
            break;
    }

    for (const PendingFile &pending : std::as_const(files)) {
        if (wasCancelled || m_cancelled->loadRelaxed() != 0) {
            wasCancelled = true;
            break;
        }

        ChecksumResult result;
        result.path = pending.absolutePath;
        result.displayName = pending.displayName;

        QFile file(pending.absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit resultReady(result); // ok stays false -- reported as a failure
            continue;
        }

        QCryptographicHash md5(QCryptographicHash::Md5);
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        QCryptographicHash sha256(QCryptographicHash::Sha256);

        bool readCancelled = false;
        while (!file.atEnd()) {
            if (m_cancelled->loadRelaxed() != 0) {
                readCancelled = true;
                break;
            }
            const QByteArray chunk = file.read(kChunkSize);
            md5.addData(chunk);
            sha1.addData(chunk);
            sha256.addData(chunk);
        }

        if (readCancelled) {
            wasCancelled = true;
            break;
        }

        result.md5 = QString::fromLatin1(md5.result().toHex());
        result.sha1 = QString::fromLatin1(sha1.result().toHex());
        result.sha256 = QString::fromLatin1(sha256.result().toHex());
        result.ok = true;
        emit resultReady(result);
    }

    emit finished(wasCancelled);
}
