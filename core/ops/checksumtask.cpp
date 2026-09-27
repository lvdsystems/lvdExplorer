#include "core/ops/checksumtask.h"

#include <QCryptographicHash>
#include <QFile>

namespace {
constexpr qint64 kChunkSize = 1 << 20; // 1 MiB
}

ChecksumTask::ChecksumTask(QStringList paths, QSharedPointer<QAtomicInt> cancelled)
    : m_paths(std::move(paths))
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

void ChecksumTask::run()
{
    bool wasCancelled = false;

    for (const QString &path : m_paths) {
        if (m_cancelled->loadRelaxed() != 0) {
            wasCancelled = true;
            break;
        }

        ChecksumResult result;
        result.path = path;

        QFile file(path);
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
