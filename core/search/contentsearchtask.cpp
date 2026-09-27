#include "core/search/contentsearchtask.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
constexpr int kBatchSize = 200;
// Past this, treat a file as "not worth grepping" rather than risk a huge
// log/media/archive file stalling the whole scan.
constexpr qint64 kMaxFileSize = 10 * 1024 * 1024;
constexpr qint64 kBinarySniffBytes = 8000;
} // namespace

ContentSearchTask::ContentSearchTask(ContentSearchOptions options, int generation, QSharedPointer<QAtomicInt> cancelled)
    : m_options(std::move(options))
    , m_generation(generation)
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);

    if (m_options.mode == ContentSearchOptions::Mode::Regex) {
        m_regex.setPattern(m_options.pattern);
        if (!m_options.caseSensitive)
            m_regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
}

bool ContentSearchTask::matches(const QString &line) const
{
    if (m_options.mode == ContentSearchOptions::Mode::Regex)
        return m_regex.isValid() && m_regex.match(line).hasMatch();
    return line.contains(m_options.pattern, m_options.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

bool ContentSearchTask::looksBinary(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return true; // unreadable -- skip it rather than fail the whole search
    return file.read(kBinarySniffBytes).contains('\0');
}

void ContentSearchTask::searchFile(const QString &filePath, QVector<ContentMatch> &batch)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        if (m_cancelled->loadRelaxed() != 0)
            return;

        const QString line = stream.readLine();
        ++lineNumber;
        if (!matches(line))
            continue;

        ContentMatch match;
        match.filePath = filePath;
        match.lineNumber = lineNumber;
        match.lineText = line.trimmed();
        batch.append(std::move(match));
    }
}

void ContentSearchTask::run()
{
    QVector<ContentMatch> batch;
    batch.reserve(kBatchSize);
    bool cancelled = false;

    const QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
    // Deliberately no FollowSymlinks, same reasoning as SearchTask (avoids
    // cyclic-symlink loops without a separate visited-path set).
    const auto iterFlags = m_options.recursive ? QDirIterator::Subdirectories : QDirIterator::IteratorFlags();

    QDirIterator it(m_options.rootPath, filters, iterFlags);
    while (it.hasNext()) {
        if (m_cancelled->loadRelaxed() != 0) {
            cancelled = true;
            break;
        }

        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.size() > kMaxFileSize || looksBinary(info.absoluteFilePath()))
            continue;

        searchFile(info.absoluteFilePath(), batch);

        if (m_cancelled->loadRelaxed() != 0) {
            cancelled = true;
            break;
        }

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
