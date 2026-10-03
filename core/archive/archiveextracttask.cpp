#include "core/archive/archiveextracttask.h"

#include "core/archive/archivereader.h"

#include <QDir>
#include <QFileInfo>

ArchiveExtractTask::ArchiveExtractTask(QString archivePath, QStringList entryPaths, QString destDir)
    : m_archivePath(std::move(archivePath))
    , m_entryPaths(std::move(entryPaths))
    , m_destDir(std::move(destDir))
{
    setAutoDelete(true);
}

void ArchiveExtractTask::run()
{
    QStringList extracted;
    QString error;
    const bool ok = ArchiveReader::extract(m_archivePath, m_entryPaths, m_destDir, &extracted, &error);

    QStringList topLevel;
    if (ok) {
        const QDir destination(m_destDir);
        for (const QString &entry : std::as_const(m_entryPaths)) {
            const QString path = destination.filePath(entry);
            if (QFileInfo::exists(path))
                topLevel.append(path);
        }
    }
    emit finished(topLevel, ok ? QString() : error);
}
