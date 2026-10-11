#include "core/fsmodel/foldersizetask.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

FolderSizeTask::FolderSizeTask(QString folderPath, int generation, QSharedPointer<QAtomicInt> cancelled)
    : m_folderPath(std::move(folderPath))
    , m_generation(generation)
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

void FolderSizeTask::run()
{
    qint64 total = 0;

    // Deliberately no FollowSymlinks, same as SearchTask/ContentSearchTask:
    // sidesteps cyclic-symlink loops without needing a visited-path set.
    // A subfolder QDirIterator can't read (permissions) is silently
    // skipped, same as every other recursive walk in this codebase --
    // matches what Explorer/du do too.
    QDirIterator it(m_folderPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (m_cancelled->loadRelaxed() != 0)
            return; // cancelled -- no result emitted for an abandoned computation
        it.next();
        total += it.fileInfo().size();
    }

    emit sizeReady(m_generation, m_folderPath, total);
}
