#include "core/archive/archiveextractalltask.h"

#include "core/archive/archivereader.h"

#include <QDir>
#include <QSet>

ArchiveExtractAllTask::ArchiveExtractAllTask(QString archivePath, QString destDir)
    : m_archivePath(std::move(archivePath))
    , m_destDir(std::move(destDir))
{
    setAutoDelete(true);
}

void ArchiveExtractAllTask::run()
{
    QVector<ArchiveEntry> all;
    QString error;
    if (!ArchiveReader::list(m_archivePath, &all, &error)) {
        emit finished({}, error);
        return;
    }

    // The "wanted" list extract() expects is top-level names only -- a
    // directory entry there pulls in everything nested under it via
    // prefix matching, same as a browsed-selection copy already relies on.
    QStringList topLevel;
    QSet<QString> seen;
    for (const ArchiveEntry &entry : std::as_const(all)) {
        const QString top = entry.path.section(QLatin1Char('/'), 0, 0);
        if (!seen.contains(top)) {
            seen.insert(top);
            topLevel.append(top);
        }
    }

    // Guarantees destDir exists even for the edge case of a genuinely
    // empty archive, where extract() below would otherwise never create it.
    QDir().mkpath(m_destDir);

    QStringList extracted;
    if (!ArchiveReader::extract(m_archivePath, topLevel, m_destDir, &extracted, &error)) {
        emit finished({}, error);
        return;
    }

    const QDir destination(m_destDir);
    QStringList resultPaths;
    resultPaths.reserve(topLevel.size());
    for (const QString &name : std::as_const(topLevel))
        resultPaths.append(destination.filePath(name));
    emit finished(resultPaths, QString());
}
