#pragma once

#include "core/search/contentmatch.h"
#include "core/search/contentsearchoptions.h"

#include <QAtomicInt>
#include <QObject>
#include <QSharedPointer>
#include <QVector>

// Orchestrates ContentSearchTask generations exactly like SearchEngine
// does for SearchTask: each start() bumps a generation and raises the
// previous task's cancel flag, and results/finish signals from a stale
// generation are silently dropped.
class ContentSearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit ContentSearchEngine(QObject *parent = nullptr);

    void start(const ContentSearchOptions &options);
    void cancel();

signals:
    void started();
    void matchesReady(const QVector<ContentMatch> &matches);
    void finished(bool wasCancelled, int totalMatches);

private slots:
    void handleMatchesReady(int generation, QVector<ContentMatch> matches);
    void handleTaskFinished(int generation, bool wasCancelled);

private:
    int m_generation = 0;
    QSharedPointer<QAtomicInt> m_cancelFlag;
    int m_totalMatches = 0;
};
