#include "core/search/searchengine.h"

#include "core/search/searchtask.h"

#include <QThreadPool>

SearchEngine::SearchEngine(QObject *parent)
    : QObject(parent)
{
}

void SearchEngine::start(const SearchOptions &options)
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);

    ++m_generation;
    m_totalMatches = 0;
    m_cancelFlag = QSharedPointer<QAtomicInt>::create(0);

    emit started();

    auto *task = new SearchTask(options, m_generation, m_cancelFlag);
    connect(task, &SearchTask::matchesReady, this, &SearchEngine::handleMatchesReady);
    connect(task, &SearchTask::finished, this, &SearchEngine::handleTaskFinished);
    QThreadPool::globalInstance()->start(task);
}

void SearchEngine::cancel()
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);
}

void SearchEngine::handleMatchesReady(int generation, QVector<FileEntry> matches)
{
    if (generation != m_generation)
        return;
    m_totalMatches += matches.size();
    emit matchesReady(matches);
}

void SearchEngine::handleTaskFinished(int generation, bool wasCancelled)
{
    if (generation != m_generation)
        return;
    emit finished(wasCancelled, m_totalMatches);
}
