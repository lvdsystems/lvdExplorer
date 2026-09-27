#include "core/search/contentsearchengine.h"

#include "core/search/contentsearchtask.h"

#include <QThreadPool>

ContentSearchEngine::ContentSearchEngine(QObject *parent)
    : QObject(parent)
{
}

void ContentSearchEngine::start(const ContentSearchOptions &options)
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);

    ++m_generation;
    m_totalMatches = 0;
    m_cancelFlag = QSharedPointer<QAtomicInt>::create(0);

    emit started();

    auto *task = new ContentSearchTask(options, m_generation, m_cancelFlag);
    connect(task, &ContentSearchTask::matchesReady, this, &ContentSearchEngine::handleMatchesReady);
    connect(task, &ContentSearchTask::finished, this, &ContentSearchEngine::handleTaskFinished);
    QThreadPool::globalInstance()->start(task);
}

void ContentSearchEngine::cancel()
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);
}

void ContentSearchEngine::handleMatchesReady(int generation, QVector<ContentMatch> matches)
{
    if (generation != m_generation)
        return;
    m_totalMatches += matches.size();
    emit matchesReady(matches);
}

void ContentSearchEngine::handleTaskFinished(int generation, bool wasCancelled)
{
    if (generation != m_generation)
        return;
    emit finished(wasCancelled, m_totalMatches);
}
