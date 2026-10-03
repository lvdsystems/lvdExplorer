#include "core/fsmodel/archivescantask.h"

#include "core/archive/archivereader.h"

ArchiveScanTask::ArchiveScanTask(QString archivePath, QString innerPath, int generation,
                                 QSharedPointer<QAtomicInt> cancelled)
    : m_archivePath(std::move(archivePath))
    , m_innerPath(std::move(innerPath))
    , m_generation(generation)
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

void ArchiveScanTask::run()
{
    QVector<ArchiveEntry> all;
    QString error;
    if (!ArchiveReader::list(m_archivePath, &all, &error)) {
        emit failed(m_generation, error);
        emit finished(m_generation, false);
        return;
    }

    const QString prefix = m_innerPath.isEmpty() ? QString() : m_innerPath + QLatin1Char('/');
    QVector<FileEntry> children;
    for (const ArchiveEntry &entry : std::as_const(all)) {
        if (m_cancelled->loadRelaxed() != 0) {
            emit finished(m_generation, true);
            return;
        }
        if (!entry.path.startsWith(prefix))
            continue;
        const QString remainder = entry.path.mid(prefix.size());
        if (remainder.isEmpty() || remainder.contains(QLatin1Char('/')))
            continue;

        FileEntry child;
        child.name = remainder;
        child.absolutePath = m_archivePath + QLatin1Char('/') + entry.path;
        child.isDir = entry.isDir;
        child.size = entry.isDir ? -1 : entry.size;
        child.modified = entry.modified;
        children.append(child);
    }

    if (!children.isEmpty())
        emit batchReady(m_generation, children);
    emit finished(m_generation, false);
}
