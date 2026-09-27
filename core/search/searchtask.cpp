#include "core/search/searchtask.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace {
constexpr int kBatchSize = 200;
}

SearchTask::SearchTask(SearchOptions options, int generation, QSharedPointer<QAtomicInt> cancelled)
    : m_options(std::move(options))
    , m_generation(generation)
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);

    if (m_options.mode == SearchOptions::Mode::Regex) {
        m_regex.setPattern(m_options.pattern);
        if (!m_options.caseSensitive)
            m_regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
}

bool SearchTask::matches(const QString &name) const
{
    if (m_options.mode == SearchOptions::Mode::Regex)
        return m_regex.isValid() && m_regex.match(name).hasMatch();
    return name.contains(m_options.pattern, m_options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

void SearchTask::run()
{
    QVector<FileEntry> batch;
    batch.reserve(kBatchSize);
    bool cancelled = false;

    const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    // Deliberately no FollowSymlinks: QDirIterator then simply won't
    // descend into symlinked directories, which sidesteps cyclic-symlink
    // loops without needing a separate visited-path set .
    const auto iterFlags = m_options.recursive ? QDirIterator::Subdirectories : QDirIterator::IteratorFlags();

    QDirIterator it(m_options.rootPath, filters, iterFlags);
    while (it.hasNext()) {
        if (m_cancelled->loadRelaxed() != 0) {
            cancelled = true;
            break;
        }

        it.next();
        const QFileInfo info = it.fileInfo();
        if (!matches(info.fileName()))
            continue;

        FileEntry entry;
        entry.name = info.fileName();
        entry.absolutePath = info.absoluteFilePath();
        entry.isDir = info.isDir();
        entry.size = entry.isDir ? -1 : info.size();
        entry.modified = info.lastModified();
        batch.append(std::move(entry));

        if (batch.size() >= kBatchSize) {
            emit matchesReady(m_generation, batch);
            batch.clear();
            batch.reserve(kBatchSize);
        }
    }

    if (!cancelled && !batch.isEmpty())
        emit matchesReady(m_generation, batch);

    emit finished(m_generation, cancelled);
}
