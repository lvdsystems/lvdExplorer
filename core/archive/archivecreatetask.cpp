#include "core/archive/archivecreatetask.h"

#include "core/archive/archivewriter.h"

ArchiveCreateTask::ArchiveCreateTask(QString zipPath, QStringList sourcePaths)
    : m_zipPath(std::move(zipPath))
    , m_sourcePaths(std::move(sourcePaths))
{
    setAutoDelete(true);
}

void ArchiveCreateTask::run()
{
    QString error;
    if (!ArchiveWriter::createZip(m_zipPath, m_sourcePaths, &error)) {
        emit finished(QString(), error);
        return;
    }
    emit finished(m_zipPath, QString());
}
