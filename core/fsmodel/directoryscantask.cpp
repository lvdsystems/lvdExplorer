#include "core/fsmodel/directoryscantask.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace {
constexpr int kBatchSize = 200;
}

DirectoryScanTask::DirectoryScanTask(QString path, int generation, QSharedPointer<QAtomicInt> cancelled)
    : m_path(std::move(path))
    , m_generation(generation)
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

void DirectoryScanTask::run()
{
    QVector<FileEntry> batch;
    batch.reserve(kBatchSize);

    QDirIterator it(m_path, QDir::AllEntries | QDir::NoDotAndDotDot);
    bool cancelled = false;

    while (it.hasNext()) {
        if (m_cancelled->loadRelaxed() != 0) {
            cancelled = true;
            break;
        }

        it.next();
        const QFileInfo info = it.fileInfo();

        FileEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDir = info.isDir();
        entry.isSymLink = info.isSymLink();
        entry.size = entry.isDir ? -1 : info.size();
        entry.modified = info.lastModified();

        batch.append(std::move(entry));

        if (batch.size() >= kBatchSize) {
            emit batchReady(m_generation, batch);
            batch.clear();
            batch.reserve(kBatchSize);
        }
    }

    if (!cancelled && !batch.isEmpty())
        emit batchReady(m_generation, batch);

    emit finished(m_generation, cancelled);
}
